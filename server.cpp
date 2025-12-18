#include <iostream>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

class TCPServer {
public:

    TCPServer(uint16_t port) : sock_fd_(-1), port_(port) {}

    ~TCPServer() {
        if (sock_fd_ >= 0) {
            close(sock_fd_);
        }
    }

    void start() {
        // Создание сокета
        sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd_ < 0) {
            throw std::runtime_error(std::string("socket: ") + strerror(errno));
        }

        // Установка опции переиспользования адреса
        int opt = 1;
        if (setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error(std::string("setsockopt: ") + strerror(errno));
        }

        // Привязка сокета к адресу
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port_);

        if (bind(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error(std::string("bind: ") + strerror(errno));
        }

        // Начало прослушивания
        if (listen(sock_fd_, 5) < 0) {
            throw std::runtime_error(std::string("listen: ") + strerror(errno));
        }

        std::cout << "Server: listening on port " << port_ << "\n";
    }

    void run() {
        while (true) {
            // Принятие подключения
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(sock_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_fd < 0) {
                std::cerr << "accept error: " << strerror(errno) << "\n";
                continue;
            }

            // Обработка клиента
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "Server: client connected from "
                    << client_ip << ":" << ntohs(client_addr.sin_port) << "\n";

            handle_client(client_fd);

            close(client_fd);
            std::cout << "Server: client disconnected\n";
        }
    }

private:
    int sock_fd_;
    uint16_t port_;

    void handle_client(int client_fd) {
        while (true) {
            std::string line = recv_line(client_fd);
            if (line.empty()) {
                std::cout << "Server: connection closed by client\n";
                break;
            }

            std::cout << "Server: received \"" << line << "\"\n";

            try {
                handle_ping(client_fd, line);
            } catch (const std::exception& ex) {
                std::cerr << "Server error: " << ex.what() << "\n";
                break;
            }
        }
    }

    std::string recv_line(int client_fd) {
        std::string result;
        char c;

        while (true) {
            ssize_t n = read(client_fd, &c, 1);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("read: ") + strerror(errno));
            }
            if (n == 0) {
                return {}; 
            }
            result.push_back(c);
            if (c == '\n') break;
        }
        return result;
    }

    void handle_ping(int client_fd, const std::string& line) {
        if (line != "PING\n") {
            send_error(client_fd, "ERROR: expected PING\n");
            throw std::runtime_error("protocol error: expected \"PING\\n\", got \"" + line + "\"");
        }
        send_pong(client_fd);
    }

    void send_pong(int client_fd) {
        const std::string msg = "PONG\n";
        const char* p = msg.c_str();
        size_t left = msg.size();

        while (left > 0) {
            ssize_t n = write(client_fd, p, left);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("write: ") + strerror(errno));
            }
            left -= n;
            p    += n;
        }

        std::cout << "Server: sent PONG\n";
    }

    void send_error(int client_fd, const std::string& msg) {
        const char* p = msg.c_str();
        size_t left = msg.size();

        while (left > 0) {
            ssize_t n = write(client_fd, p, left);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("write (error): ") + strerror(errno));
            }
            left -= n;
            p += n;
        }

        std::cout << "Server: sent error message: " << msg;
    }
};

int main() {
    try {
        TCPServer server(54321);
        server.start();
        server.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}