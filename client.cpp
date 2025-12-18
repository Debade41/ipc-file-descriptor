#include <iostream>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

class TCPClient {
public:
    TCPClient(const std::string& server_ip, uint16_t port)
        : sock_fd_(-1), server_ip_(server_ip), port_(port) {}

    ~TCPClient() {
        if (sock_fd_ >= 0) {
            close(sock_fd_);
        }
    }

    void connect_to_server() {
        sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd_ < 0) {
            throw std::runtime_error(std::string("socket: ") + strerror(errno));
        }

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port_);

        if (inet_pton(AF_INET, server_ip_.c_str(), &addr.sin_addr) <= 0) {
            throw std::runtime_error(std::string("inet_pton: ") + strerror(errno));
        }

        if (connect(sock_fd_, reinterpret_cast<sockaddr*>(&addr),
                    sizeof(addr)) < 0) {
            throw std::runtime_error(std::string("connect: ") + strerror(errno));
        }

        std::cout << "Client: connected to " << server_ip_ << ":" << port_ << "\n";
    }

    void run_interactive() {
        std::string input;

        while (true) {
            std::cout << "Enter message (PING to get PONG, EXIT to quit): ";
            if (!std::getline(std::cin, input)) {
                std::cout << "\nClient: stdin closed, exiting\n";
                break;
            }

            if (input == "EXIT" || input == "exit") {
                std::cout << "Client: exiting by user request\n";
                break;
            }

            std::string msg = input + "\n";

            try {
                send_message(msg);
            } catch (const std::exception& ex) {
                std::cerr << "Client: write error: " << ex.what() << "\n";
                break;
            }

            std::string resp;
            try {
                resp = recv_line();
            } catch (const std::exception& ex) {
                std::cerr << "Client: read error: " << ex.what() << "\n";
                break;
            }

            if (resp.empty()) {
                std::cout << "Client: server closed connection\n";
                break;
            }

            if (resp == "PONG\n") {
                std::cout << "Client: got PONG\n";
            } else {
                std::cerr << "Client: server reported error: " << resp;
                break;
            }
        }
    }

private:
    int sock_fd_;
    std::string server_ip_;
    uint16_t port_;

    void send_message(const std::string& msg) {
        const char* p = msg.c_str();
        size_t left = msg.size();

        while (left > 0) {
            ssize_t n = write(sock_fd_, p, left);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("write: ") + strerror(errno));
            }
            left -= static_cast<size_t>(n);
            p += n;
        }
    }

    std::string recv_line() {
        std::string result;
        char c;

        while (true) {
            ssize_t n = read(sock_fd_, &c, 1);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("read: ") + strerror(errno));
            }
            if (n == 0) {
                return std::string();
            }
            result.push_back(c);
            if (c == '\n') break;
        }
        return result;
    }
};

int main() {
    try {
        TCPClient client("127.0.0.1", 54321);
        client.connect_to_server();
        client.run_interactive();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}