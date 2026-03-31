#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include "sqliteDB/sqlite3.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Глобальный флаг для мягкой остановки сервера
std::atomic<bool> keep_running(true);

void signal_handler(int) {
    keep_running = false;
    // При получении Ctrl+C мы могли бы закрыть слушающий сокет здесь,
    // чтобы accept() вышел с ошибкой и цикл завершился.
}

void handle_user(int client_soc, const std::string ip_addr) {
    sqlite3 *db;
    // Открываем базу внутри потока
    if (sqlite3_open("my_database.db", &db) != SQLITE_OK) {
        std::cerr << "DB error: " << sqlite3_errmsg(db) << std::endl;
        close(client_soc);
        return;
    }

    // Включаем WAL режим и ожидание, чтобы избежать ошибок SQLITE_BUSY при записи из разных потоков
    sqlite3_busy_timeout(db, 5000);

    char buf[1025];
    while (keep_running) {
        int bytes_read = recv(client_soc, buf, 1024, 0);
        if (bytes_read > 0) {
            std::string message(buf, bytes_read);
            std::cout << "[" << ip_addr << "]: " << message << std::endl;

            send(client_soc, "OK", 2, 0);

            const char *sql = "INSERT INTO messages (content, client_ip) VALUES (?, ?);";
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, message.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, ip_addr.c_str(), -1, SQLITE_TRANSIENT);

                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    std::cerr << "Ошибка записи: " << sqlite3_errmsg(db) << std::endl;
                }
                sqlite3_finalize(stmt);
            }
        }
        else if (bytes_read == 0) {
            std::cout << "Клиент " << ip_addr << " отключился." << std::endl;
            break;
        }
        else {
            break;
        }
    }

    sqlite3_close(db);
    close(client_soc);
}

int main() {
    std::signal(SIGINT, signal_handler);

    sqlite3 *db_setup;
    sqlite3_open("my_database.db", &db_setup);
    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY, content TEXT, client_ip TEXT);";
    sqlite3_exec(db_setup, createTableSQL, nullptr, nullptr, nullptr);
    sqlite3_close(db_setup); // Закрываем стартовое соединение

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(sock, 10);
    std::cout << "Сервер запущен на порту 8080..." << std::endl;

    while (keep_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client = accept(sock, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
        if (client < 0) {
            if (keep_running) perror("accept");
            break;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);

        // ВОТ ТОТ САМЫЙ КУСОК:
        // Создаем поток и сразу отсоединяем его (detach), чтобы не копить ресурсы
        std::thread(handle_user, client, std::string(ip_str)).detach();
    }

    close(sock);
    std::cout << "Сервер завершил работу." << std::endl;
    return 0;
}