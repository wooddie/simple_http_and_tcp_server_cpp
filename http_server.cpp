#define CPPHTTPLIB_NO_EXCEPTIONS
#include "lib/httplib.h"
#include <openssl/sha.h>
#include "sqlite3.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <../lib/json-develop/single_include/nlohmann/json.hpp> // Популярная библиотека для JSON
using json = nlohmann::json;

// ---------- SHA256 ----------
std::string sha256(const std::string &str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *) str.c_str(), str.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int) hash[i];
    }
    return ss.str();
}

// ---------- HTML LOAD ----------
std::string load_html(const std::string &path) {
    std::ifstream file(path);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

// ---------- TEMPLATE REPLACE ----------
void replace_all(std::string &data, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = data.find(from, pos)) != std::string::npos) {
        data.replace(pos, from.length(), to);
        pos += to.length();
    }
}

// ---------- DB ----------
void exec_sql(sqlite3 *db, const char *sql) {
    char *err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << err << std::endl;
        sqlite3_free(err);
    }
}

void init_db(sqlite3 *db) {
    exec_sql(db, "PRAGMA foreign_keys = ON;");

    exec_sql(db,
             "CREATE TABLE IF NOT EXISTS users ("
             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "iin TEXT UNIQUE,"
             "fio TEXT,"
             "phone TEXT,"
             "email TEXT UNIQUE,"
             "password TEXT);");

    exec_sql(db,
             "CREATE TABLE IF NOT EXISTS generation_requests ("
             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "user_id INTEGER,"
             "content_type TEXT,"
             "prompt_text TEXT,"
             "status TEXT DEFAULT 'pending',"
             "created_at TEXT,"
             "FOREIGN KEY(user_id) REFERENCES users(id));");
}

// ---------- MAIN ----------
int main() {
    sqlite3 *db;
    sqlite3_open("projectDB.db", &db);
    init_db(db);

    httplib::Server server;

    // ROOT
    server.Get("/", [](const httplib::Request &, httplib::Response &res) {
        res.set_content(load_html("../templates/index.html"), "text/html");
    });

    // GET DATA
    server.Get("/get_messages", [db](const httplib::Request &req, httplib::Response &res) {
        json j_list = json::array();
        sqlite3_stmt *stmt;
        // Соединяем таблицы, чтобы видеть имя юзера вместо его ID
        const char *sql = "SELECT gr.id, gr.created_at, u.fio, gr.content_type, gr.prompt_text, gr.status "
                "FROM generation_requests gr JOIN users u ON gr.user_id = u.id;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                json item;
                item["id"] = (const char *) sqlite3_column_text(stmt, 0);
                item["created_at"] = (const char *) sqlite3_column_text(stmt, 1);
                item["fio"] = (const char *) sqlite3_column_text(stmt, 2);
                item["content_type"] = (const char *) sqlite3_column_text(stmt, 3);
                item["prompt_text"] = (const char *) sqlite3_column_text(stmt, 4);
                item["status"] = (const char *) sqlite3_column_text(stmt, 5);
                j_list.push_back(item);
            }
            sqlite3_finalize(stmt);
        }
        res.set_content(j_list.dump(), "application/json");
    });

    // POST DATA FROM ADMIN
    server.Post("/update_status", [db](const httplib::Request &req, httplib::Response &res) {
        std::string id = req.get_param_value("id");
        std::string status = req.get_param_value("status");

        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "UPDATE generation_requests SET status=? WHERE id=?", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, std::stoi(id));

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        res.set_content("OK", "text/plain");
    });

    // REGISTER
    server.Post("/register", [db](const httplib::Request &req, httplib::Response &res) {
        std::string iin = req.get_param_value("iin");
        std::string fio = req.get_param_value("fio");
        std::string phone = req.get_param_value("phone");
        std::string email = req.get_param_value("email");
        std::string password = req.get_param_value("password");

        // 1. Валидация ИИН (ровно 12 цифр)
        if (!std::regex_match(iin, std::regex("^\\d{12}$"))) {
            res.status = 400;
            res.set_content("ИИН должен состоять ровно из 12 цифр", "text/plain; charset=utf-8");
            return;
        }

        // 2. Валидация ФИО (Кириллица + казахские буквы)
        if (std::regex_search(fio, std::regex("[a-zA-Z0-9]"))) {
            // Если нашли латиницу или цифры — выдаем ошибку
            res.status = 400;
            res.set_content("Ошибка: только кириллица", "text/plain; charset=utf-8");
            return;
        }

        // 3. Валидация телефона +7(7XX)-XXX-XX-XX
        if (!std::regex_match(phone, std::regex("^\\+7\\(7\\d{2}\\)-\\d{3}-\\d{2}-\\d{2}$"))) {
            res.status = 400;
            res.set_content("Формат телефона: +7(7XX)-XXX-XX-XX", "text/plain; charset=utf-8");
            return;
        }

        // 4. Длина пароля
        if (password.length() < 8) {
            res.status = 400;
            res.set_content("Пароль должен быть не менее 8 символов", "text/plain; charset=utf-8");
            return;
        }

        // Если всё ок — хешируем и сохраняем
        std::string pass_hash = sha256(password);

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO users (iin,fio,phone,email,password) VALUES (?,?,?,?,?)";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, iin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, fio.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, phone.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, pass_hash.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                res.set_content("OK <a href='/'>login</a>", "text/html");
            } else {
                res.status = 400;
                res.set_content("Ошибка БД (возможно, ИИН уже зарегистрирован)", "text/plain; charset=utf-8");
            }
            sqlite3_finalize(stmt);
        }
    });

    // LOGIN
    server.Post("/login", [db](const httplib::Request &req, httplib::Response &res) {
        std::string email = req.get_param_value("email");
        std::string pass = sha256(req.get_param_value("password"));

        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM users WHERE email=? AND password=?", -1, &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pass.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            res.set_redirect("../home?user_id=" + std::to_string(id));
        } else {
            res.set_content("bad login", "text/html");
        }

        sqlite3_finalize(stmt);
    });

    // HOME
    server.Get("/home", [db](const httplib::Request &req, httplib::Response &res) {
        std::string user_id = req.get_param_value("user_id");

        std::string html = load_html("../templates/home.html");

        std::string requests;

        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
                           "SELECT content_type,prompt_text,status FROM generation_requests WHERE user_id=?",
                           -1, &stmt, nullptr);

        sqlite3_bind_int(stmt, 1, std::stoi(user_id));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string s = (char *) sqlite3_column_text(stmt, 2); // 'pending', 'approved', etc.
            std::string displayStatus = (s == "pending")
                                            ? "Жаңа (Новый)"
                                            : (s == "approved")
                                                  ? "Мақұлданды"
                                                  : "Қабылданбады";
            std::string t = (char *) sqlite3_column_text(stmt, 0);
            std::string p = (char *) sqlite3_column_text(stmt, 1);

            // Добавляем HTML-обертку с классом для CSS
            requests += "<div class='request-card status-" + s + "'>";
            requests += "<strong>" + t + "</strong><br>";
            requests += "<span>" + p + "</span><br>";
            requests += "<i>Статус: " + s + "</i>";
            requests += "</div>";
        }

        sqlite3_finalize(stmt);

        replace_all(html, "{{user_id}}", user_id);
        replace_all(html, "{{requests}}", requests);

        res.set_content(html, "text/html");
    });

    // GENERATE
    server.Post("/generate", [db](const httplib::Request &req, httplib::Response &res) {
        std::string user_id = req.get_param_value("user_id");
        std::string prompt = req.get_param_value("prompt");
        std::string type = req.get_param_value("type");

        // 1. Валидация промпта (длина 20-500)
        if (prompt.length() < 20 || prompt.length() > 500) {
            res.status = 400; // Обязательно ставим статус ошибки
            res.set_content("Промпт должен содержать от 20 до 500 символов", "text/plain; charset=utf-8");
            return;
        }

        // 2. Проверка на пустоту типа контента
        if (type != "Изображение" && type != "Видео") {
            res.status = 400;
            res.set_content("Выберите тип контента (Изображение/Видео)", "text/plain; charset=utf-8");
            return;
        }

        // Если всё ок — пишем в базу
        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO generation_requests(user_id, content_type, prompt_text, status, created_at) "
                "VALUES (?, ?, ?, 'pending', datetime('now'))";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, std::stoi(user_id));
            sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, prompt.c_str(), -1, SQLITE_TRANSIENT);

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            // После успешного создания заказа лучше перенаправить
            // или вернуть статус 201
            res.set_redirect("/home?user_id=" + user_id);
        }
    });

    std::cout << "http://localhost:8080\n";
    server.set_mount_point("/styles", "../templates/styles");
    server.set_mount_point("/scripts", "../templates/scripts");
    server.listen("0.0.0.0", 8080);

    sqlite3_close(db);
}
