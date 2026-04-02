#define CPPHTTPLIB_NO_EXCEPTIONS
#include "lib/httplib.h"
#include <openssl/sha.h>
#include "sqlite3.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

// ---------- SHA256 ----------
std::string sha256(const std::string& str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)str.c_str(), str.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// ---------- HTML LOAD ----------
std::string load_html(const std::string& path) {
    std::ifstream file(path);
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

// ---------- TEMPLATE REPLACE ----------
void replace_all(std::string& data, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = data.find(from, pos)) != std::string::npos) {
        data.replace(pos, from.length(), to);
        pos += to.length();
    }
}

// ---------- DB ----------
void exec_sql(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << err << std::endl;
        sqlite3_free(err);
    }
}

void init_db(sqlite3* db) {
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
    sqlite3* db;
    sqlite3_open("projectDB.db", &db);
    init_db(db);

    httplib::Server server;

    // ROOT
    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(load_html("../templates/index.html"), "text/html");
    });

    // REGISTER
    server.Post("/register", [db](const httplib::Request& req, httplib::Response& res) {
        std::string iin = req.get_param_value("iin");
        std::string fio = req.get_param_value("fio");
        std::string phone = req.get_param_value("phone");
        std::string email = req.get_param_value("email");
        std::string pass = sha256(req.get_param_value("password"));

        sqlite3_stmt* stmt;
        const char* sql = "INSERT INTO users (iin,fio,phone,email,password) VALUES (?,?,?,?,?)";

        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, iin.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, fio.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, phone.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, pass.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE)
            res.set_content("OK <a href='/'>login</a>", "text/html");
        else
            res.set_content("Error", "text/html");

        sqlite3_finalize(stmt);
    });

    // LOGIN
    server.Post("/login", [db](const httplib::Request& req, httplib::Response& res) {
        std::string email = req.get_param_value("email");
        std::string pass = sha256(req.get_param_value("password"));

        sqlite3_stmt* stmt;
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
    server.Get("/home", [db](const httplib::Request& req, httplib::Response& res) {
        std::string user_id = req.get_param_value("user_id");

        std::string html = load_html("../templates/home.html");

        std::string requests;

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db,
            "SELECT content_type,prompt_text,status FROM generation_requests WHERE user_id=?",
            -1, &stmt, nullptr);

        sqlite3_bind_int(stmt, 1, std::stoi(user_id));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string t = (char*)sqlite3_column_text(stmt, 0);
            std::string p = (char*)sqlite3_column_text(stmt, 1);
            std::string s = (char*)sqlite3_column_text(stmt, 2);

            requests += "<p>" + t + " | " + p + " | " + s + "</p>";
        }

        sqlite3_finalize(stmt);

        replace_all(html, "{{user_id}}", user_id);
        replace_all(html, "{{requests}}", requests);

        res.set_content(html, "text/html");
    });

    // GENERATE
    server.Post("/generate", [db](const httplib::Request& req, httplib::Response& res) {
        std::string user_id = req.get_param_value("user_id");

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db,
            "INSERT INTO generation_requests(user_id,content_type,prompt_text,created_at) VALUES (?,?,?,datetime('now'))",
            -1, &stmt, nullptr);

        sqlite3_bind_int(stmt, 1, std::stoi(user_id));
        sqlite3_bind_text(stmt, 2, req.get_param_value("type").c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, req.get_param_value("prompt").c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        res.set_redirect("/home?user_id=" + user_id);
    });

    std::cout << "http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);

    sqlite3_close(db);
}