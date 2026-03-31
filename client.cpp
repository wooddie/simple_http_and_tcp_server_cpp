#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <netinet/in.h> // Для sockaddr_in
#include <arpa/inet.h>  // Для htons и inet_pton

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Использование: ./client <ip> <message>" << std::endl;
        return 1;
    }

    char *server_ip = argv[1];
    char *message = argv[2];

    int client = socket(AF_INET, SOCK_STREAM, 0); // Создаём TCP сокет

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    inet_pton(AF_INET, server_ip, &addr.sin_addr);

    if (client < 0)
    {
        perror("socket");
        return 1;
    }
    else
    {
        std::cout << "Сокет успешно создался" << std::endl;
    }

    if (connect(client, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        std::cout << "Не удалось подключится" << std::endl;
        return 1;
    }
    else
    {
        send(client, message, strlen(message), 0);
        std::cout << "Отправил ваше сообщение: " << message << std::endl;
        std::cout << "Задействовано памяти: " << strlen(message) << " байт" << std::endl;
    }

    close(client);
    return 0;
}