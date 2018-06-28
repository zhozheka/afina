
#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include "addConnection.h"
#include <unistd.h>


namespace Afina {
namespace Network {
namespace NonBlocking {

    addConnection::addConnection() {}
    addConnection::~addConnection() {}

    addConnection::addConnection(std::shared_ptr<Afina::Storage> ps, int sock): pStorage(ps), socket(sock) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
        parser = Protocol::Parser();
        is_parsed = false;
        cState = State::kRun;
    }

    void addConnection::routine() {
        //std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
        size_t buf_size = 1024;
        char buffer[buf_size];
        std::string out;
        size_t parsed = 0;
        size_t curr_pos = 0;
        ssize_t n_read = 0;
        uint32_t body_size = 0;


        n_read = recv(socket, buffer + curr_pos, buf_size - curr_pos, 0);

        if (n_read == 0) {
            close(socket);
            throw std::runtime_error("Client disconnected");
        } else if (n_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK){ // In data ended.
                if (cState == State::kStopping){
                    // shut down server
                    close(socket);
                    std::cout << "Socket closed" << std::endl;
                    return;
                }
                return;
            } else {
                close(socket);
                throw std::runtime_error("User irrespectively disconnected");
            }
        }
        curr_pos += n_read;

        while (parsed < curr_pos) {
            try {
                is_parsed = parser.Parse(buffer, curr_pos, parsed);
            } catch (std::runtime_error &err) {
                std::cout << "Parser error" << std::endl;
                return;
            }
            if (is_parsed) {
                size_t body_read = curr_pos - parsed;
                memcpy(buffer, buffer + parsed, body_read);
                memset(buffer + body_read, 0, parsed);
                curr_pos = body_read;

                auto cmd = parser.Build(body_size);

                if (body_size <= curr_pos) {
                    char args[body_size + 1];
                    memcpy(args, buffer, body_size);
                    args[body_size] = '\0';
                    if (body_size) {
                        memcpy(buffer, buffer + body_size + 2, curr_pos - body_size - 2);
                        memset(buffer + curr_pos - body_size - 2, 0, body_size);
                        curr_pos -= body_size + 2;
                    }
                    try {
                        cmd->Execute(*(pStorage.get()), args, out);
                        out += "\r\n";
                    } catch (std::runtime_error &err) {
                        //out = std::string("SERVER_ERROR : ") + err.what() + "\r\n";
                        std::cout << "Can't execute: " << err.what() << std::endl;
                    }
                    parser.Reset();
                    is_parsed = false;
                    parsed = 0;
                }
            }


        }
        if (cState == State::kStopping){
            std::cout << "Stopping server" << std::endl;
            close(socket);
        }
    }
} //Nonblocking
} //Network
} //Afina
