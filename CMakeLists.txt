cmake_minimum_required(VERSION 3.16) # Для Qt 6 лучше минимум 3.16
project(cpp_server)

set(CMAKE_CXX_STANDARD 17) # Qt 6 требует минимум C++ 17

# --- Включаем магию Qt (АВТО-генерация кода) ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- Находим пакеты ----
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
# Добавляем Qt (Core для логики, Network для сети, Widgets если будет GUI)
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)
# Добавь Sql в поиск пакетов
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

# ---- SQLite ----
include_directories(sqliteDB)
add_library(sqlite3_lib sqliteDB/sqlite3.c)

# ---- SERVER ----
add_executable(server server.cpp)
target_link_libraries(server
        sqlite3_lib
        Threads::Threads
        Qt6::Core
        Qt6::Network
)

# ---- CLIENT (Допустим, тут будет интерфейс Qt) ----
# Добавляем MACOSX_BUNDLE, чтобы на Mac это было приложение .app
add_executable(client MACOSX_BUNDLE client.cpp)
target_link_libraries(client
        Threads::Threads
        Qt6::Widgets
        Qt6::Network
)

# ---- HTTP SERVER ----
add_executable(http_server http_server.cpp)
target_link_libraries(http_server
        sqlite3_lib
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::SSL
        Qt6::Core
)

# Создай отдельный исполняемый файл (App Bundle для macOS)
add_executable(admin MACOSX_BUNDLE admin.cpp)

# Привяжи библиотеки
target_link_libraries(admin PRIVATE Qt6::Widgets Qt6::Sql)