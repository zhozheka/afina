#ifndef AFINA_NETWORK_NONBLOCKING_CONNECTION_CPP
#define AFINA_NETWORK_NONBLOCKING_CONNECTION_CPP

#include <memory>
#include <atomic>
#include <pthread.h>
#include <vector>
#include <string>
#include <unistd.h>
#include <afina/execute/Command.h>
#include "../../protocol/Parser.h"
#include <cstring>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>

#define BUF_SIZE 1024

namespace Afina {

class Storage;

namespace Network {
namespace NonBlocking {

enum class State {
    Read,
    Body,
    Write
};

class Connection {
public:
    Connection(int _socket, std::atomic<bool>& running, std::shared_ptr<Afina::Storage> ps) : socket(_socket), state(State::Read), pStorage(ps), running(running) {
    }
    ~Connection(void) {
        close(socket);
    }

public:
    int socket;
    std::string body;
    std::string out;
    Protocol::Parser parser;

    std::atomic<bool>& running;
    std::unique_ptr<Execute::Command> cmd;
    std::shared_ptr<Afina::Storage> pStorage;

    State state;
    char buffer[BUF_SIZE];
    size_t position;
    uint32_t body_size;
    size_t bytes_sent_total = 0;




    bool Proc() {

        while (running.load()) {
            try {
                // read command
               if (state == State::Read) {
                   size_t parsed = 0;
                   while (!parser.Parse(buffer, position, parsed)) {
                       std::memmove(buffer, buffer + parsed, position - parsed);
                       position -= parsed;

                       ssize_t n_read = recv(socket, buffer + position, BUF_SIZE - position, 0);
                       if (n_read <= 0) {
                           if ((errno == EWOULDBLOCK || errno == EAGAIN) && n_read < 0 && running.load()) {
                               return true;
                           } else {
                               return false;
                           }
                       }

                       position += n_read;
                   }
                   std::memmove(buffer, buffer + parsed, position - parsed);
                   position -= parsed;

                   cmd = parser.Build(body_size);
                   body_size += 2;
                   parser.Reset();

                   body.clear();
                   state = State::Body;
               }
               // read body
               if (state == State::Body) {
                   if (body_size > 2) {
                       while (body_size > position) {
                           body.append(buffer, position);
                           body_size -= position;
                           position = 0;


                           ssize_t n_read = recv(socket, buffer, BUF_SIZE, 0);
                           if (n_read <= 0) {
                               if ((errno == EWOULDBLOCK || errno == EAGAIN) && n_read < 0 && running.load()) {
                                   return true;
                               } else {
                                   return false;
                               }
                           }

                           position = n_read;
                       }

                       body.append(buffer, body_size);
                       std::memmove(buffer, buffer + body_size, position - body_size);
                       position -= body_size;

                       body = body.substr(0, body.length() - 2);
                   }

                   cmd->Execute(*pStorage, body, out);
                   out.append("\r\n");
                   state = State::Write;
               }
           } catch (std::runtime_error &e) {
               out = std::string("SERVER_ERROR ") + e.what() + std::string("\r\n");
               std::cout << "error catched: " << e.what() <<  std::endl;
               position = 0;
               parser.Reset();
               //return false;
               state = State::Write;
           }
           if (state == State::Write) {
               if (out.size() > 2) {
                   while (bytes_sent_total < out.size()) {

                       ssize_t n_sent = send(socket, out.data() + bytes_sent_total, out.size() - bytes_sent_total, 0);
                       if (n_sent < 0) {
                            if ((errno == EWOULDBLOCK || errno == EAGAIN) && running.load()) {
                                return true;
                            } else {
                                return false;
                            }
                        }

                       bytes_sent_total += n_sent;
                   }
               }
               bytes_sent_total = 0;
               state = State::Read;
           }
        }
        return false;
    }
};
} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_CONNECTION_CPP
