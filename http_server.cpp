#define CPPHTTPLIB_NO_EXCEPTIONS
#include "lib/httplib.h"
#include <openssl/evp.h>
#include "sqlite3.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <../lib/json-develop/single_include/nlohmann/json.hpp>
using json = nlohmann::json;

// ---------- GET COOKIE ----------
std::string getCookie(const httplib::Request &req, std::string key) {
    if (!req.has_header("Cookie")) { return ""; }

    std::string cookie = req.get_header_value("Cookie");
    std::stringstream ss(cookie);
    std::string item;

    while (std::getline(ss, item, ';')) {
        size_t pos = item.find('=');
        if (pos != std::string::npos) {
            std::string k = item.substr(0, pos);
            std::string v = item.substr(pos + 1);

            // убираем пробелы
            while (!k.empty() && k[0] == ' ') k.erase(0, 1);

            if (k == key) return v;
        }
    }
    return "";
}


// ---------- SHA256 ----------
std::string sha256(const std::string &str) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    EVP_DigestInit_ex(context, md, nullptr);
    EVP_DigestUpdate(context, str.c_str(), str.size());
    EVP_DigestFinal_ex(context, hash, &lengthOfHash);
    EVP_MD_CTX_free(context);


    std::stringstream ss;
    for (int i = 0; i < lengthOfHash; i++) {
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

        if (!std::regex_match(iin, std::regex("^\\d{12}$"))) {
            res.status = 400;
            res.set_content("ИИН должен состоять ровно из 12 цифр", "text/plain; charset=utf-8");
            return;
        }

        if (std::regex_search(fio, std::regex("[a-zA-Z0-9]"))) {
            // Если нашли латиницу или цифры — выдаем ошибку
            res.status = 400;
            res.set_content("Ошибка: только кириллица", "text/plain; charset=utf-8");
            return;
        }

        if (!std::regex_match(phone, std::regex("^\\+7\\(\\d{3}\\)-\\d{3}-\\d{2}-\\d{2}$"))) {
            res.status = 400;
            res.set_content("Формат телефона: +7(7XX)-XXX-XX-XX", "text/plain; charset=utf-8");
            return;
        }

        if (password.length() < 8) {
            res.status = 400;
            res.set_content("Пароль должен быть не менее 8 символов", "text/plain; charset=utf-8");
            return;
        }

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
                // ВЫВОДИМ ОШИБКУ В КОНСОЛЬ СЕРВЕРА
                std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;

                res.status = 400;
                res.set_content("reg_error", "text/plain");
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
            std::string userIdStr = std::to_string(id);
            res.set_header("Set-Cookie", "session_id=" + userIdStr + "; Path=/; HttpOnly");
            res.set_redirect("/home");
        } else {
            res.status = 401;
            res.set_content("auth_error", "text/plain");
        }

        sqlite3_finalize(stmt);
    });

    // HOME
    server.Get("/home", [db](const httplib::Request &req, httplib::Response &res) {
        std::string user_id = "";

        // 1. Пытаемся достать user_id из куки
        if (req.has_header("Cookie")) {
            std::string cookie_header = req.get_header_value("Cookie");

            // Простой парсинг куки "session_id=X"
            size_t pos = cookie_header.find("session_id=");
            if (pos != std::string::npos) {
                user_id = cookie_header.substr(pos + 11); // 11 - это длина "session_id="
                // Если в куке есть другие параметры после ID, отрезаем их до первой точки с запятой
                size_t end_pos = user_id.find(';');
                if (end_pos != std::string::npos) {
                    user_id = user_id.substr(0, end_pos);
                }
            }
        }

        // 2. Если ID не найден или пустой — выгоняем на страницу логина
        if (user_id.empty()) {
            res.set_redirect("/");
            return;
        }

        // 2.1 ПРОВЕРКА: Существует ли такой пользователь в базе?
        sqlite3_stmt *check_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM users WHERE id=?", -1, &check_stmt, nullptr);
        sqlite3_bind_int(check_stmt, 1, std::stoi(user_id));

        if (sqlite3_step(check_stmt) != SQLITE_ROW) {
            // Если пользователя с таким ID нет — чистим куку и выгоняем
            sqlite3_finalize(check_stmt);
            res.set_header("Set-Cookie", "session_id=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
            res.set_redirect("/");
            return;
        }
        sqlite3_finalize(check_stmt);

        // 3. Если мы здесь, значит user_id у нас из надежного источника (куки)
        std::string html = load_html("../templates/home.html");
        std::string requests_html = "";

        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT content_type, prompt_text, status FROM generation_requests WHERE user_id=?", -1,
                           &stmt, nullptr);

        // Безопасно привязываем ID из куки
        sqlite3_bind_int(stmt, 1, std::stoi(user_id));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string type = (const char *) sqlite3_column_text(stmt, 0);
            std::string prompt = (const char *) sqlite3_column_text(stmt, 1);
            std::string status = (const char *) sqlite3_column_text(stmt, 2);

            requests_html += "<div class='request-card status-" + status + "'>";
            requests_html += "  <div class='meta'>";
            requests_html += "    <span class='type-badge'>" + type + "</span>";
            requests_html += "    <span class='status-badge' data-key='status_" + status + "'>" + status + "</span>";
            requests_html += "  </div>";
            requests_html += "  <div class='prompt'>" + prompt + "</div>";
            requests_html += "</div>";
        }

        sqlite3_finalize(stmt);

        replace_all(html, "{{user_id}}", user_id);
        replace_all(html, "{{requests}}", requests_html);

        res.set_content(html, "text/html");
    });

    // GENERATE
    server.Post("/generate", [db](const httplib::Request &req, httplib::Response &res) {
        auto user = getCookie(req, "session_id");

        if (user.empty()) {
            res.set_redirect("/");
            return;
        }

        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM users WHERE id=?", -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, std::stoi(user));

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);

            // чистим куку
            res.set_header("Set-Cookie", "user_id=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
            res.set_redirect("/");
            return;
        }

        sqlite3_finalize(stmt);

        std::string user_id = user;
        std::string prompt = req.get_param_value("prompt");
        std::string type = req.get_param_value("type");

        if (prompt.length() < 20 || prompt.length() > 500) {
            res.status = 400; // Обязательно ставим статус ошибки
            res.set_content("Промпт должен содержать от 20 до 500 символов", "text/plain; charset=utf-8");
            return;
        }

        if (type != "Изображение" && type != "Видео") {
            res.status = 400;
            res.set_content("Выберите тип контента (Изображение/Видео)", "text/plain; charset=utf-8");
            return;
        }

        const char *sql = "INSERT INTO generation_requests(user_id, content_type, prompt_text, status, created_at) "
                "VALUES (?, ?, ?, 'pending', datetime('now'))";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, std::stoi(user_id));
            sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, prompt.c_str(), -1, SQLITE_TRANSIENT);

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            res.set_redirect("/home?user_id=" + user_id);
        }
    });

    std::cout << "http://localhost:8080\n";
    server.set_mount_point("/styles", "../templates/styles");
    server.set_mount_point("/scripts", "../templates/scripts");
    server.listen("0.0.0.0", 8080);

    sqlite3_close(db);
}
