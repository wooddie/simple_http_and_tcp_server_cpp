//
// Created by Канапия on 30.03.2026.
//
#define CPPHTTPLIB_NO_EXCEPTIONS
#include "lib/httplib.h"
#include "sqlite3.h"
#include <iomanip>
#include <sstream>

int main() {
    sqlite3 *db;

    if (sqlite3_open("projectDB.db", &db) != SQLITE_OK) {
        std::cerr << "DB error: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    const char *createTableSQL =
            "CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY, user_name TEXT, coordinates TEXT, client_ip TEXT, created_at TEXT);";
    sqlite3_exec(db, createTableSQL, nullptr, nullptr, nullptr);

    httplib::Server server;

    server.Get("/hi", [](const httplib::Request &req, httplib::Response &res) {
        res.set_content(R"(
        <div style="display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; text-align: center; gap: 20px;">
            <h1>Hello user!</h1>

            <form action="/PostData" method="POST">
                <input name="user_name" type="text" placeholder="Name">
                <input name="coordinates" type="text" placeholder="Coordinates">
                <button type="submit">Send coordinates</button>
            </form>
        </div>
    )", "text/html");
    });


    server.Post("/PostData", [db](const httplib::Request &req, httplib::Response &res) {
        std::string user_name = req.get_param_value("user_name");
        std::string coordinates = req.get_param_value("coordinates");
        std::string client_ip = req.remote_addr;

        std::cout << "Получен POST от " << client_ip << ": " << coordinates.c_str() << " : " << user_name.c_str() << std::endl;

        const char *sql = "INSERT INTO messages (user_name, coordinates, client_ip, created_at) VALUES (?, ?, ?, ?);";
        sqlite3_stmt *stmt;

        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        std::string current_time = oss.str();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, user_name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, coordinates.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, client_ip.c_str(), -1, SQLITE_TRANSIENT); // IP сюда
            sqlite3_bind_text(stmt, 4, current_time.c_str(), -1, SQLITE_TRANSIENT); // Время сюда

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                res.status = 201; // Created
                res.set_content("Data saved", "text/plain");
                res.set_redirect("/hi"); // Пользователь сразу увидит форму снова
            } else {
                res.status = 500;
                res.set_content("Error saving to DB", "text/plain");
            }
            sqlite3_finalize(stmt);
        } else {
            res.status = 400;
            res.set_content("SQL Error", "text/plain");
        }
    });

    std::cout << "HTTP Сервер запущен на http://localhost:8080" << std::endl;

    server.listen("0.0.0.0", 8080);

    sqlite3_close(db);

    return 0;
}
