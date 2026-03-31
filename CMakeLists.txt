cmake_minimum_required(VERSION 3.10)
project(cpp_server)

set(CMAKE_CXX_STANDARD 14)

# Находим пакет потоков (обязательно для httplib)
find_package(Threads REQUIRED)

# SQLite как библиотека (исходник .c)
# Добавляем sqlite3.h в include, чтобы сервер его видел
include_directories(sqliteDB)
add_library(sqlite3_lib sqliteDB/sqlite3.c)

# СЕРВЕР
add_executable(server server.cpp)
# Линкуем sqlite3 и потоки
target_link_libraries(server sqlite3_lib Threads::Threads)

# КЛИЕНТ
add_executable(client client.cpp)
target_link_libraries(client Threads::Threads)

# HTTP_SERVER (если это отдельный файл)
add_executable(http_server http_server.cpp)
target_link_libraries(http_server sqlite3_lib Threads::Threads)