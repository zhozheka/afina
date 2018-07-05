#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

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
        auto buffer = this->buffer;
        int socket = this->socket;

        while (running.load()) {
            try {
                // read command
               if (this->state == State::Read) {
                   size_t parsed = 0;
                   while (!this->parser.Parse(buffer, this->position, parsed)) {
                       std::memmove(buffer, buffer + parsed, this->position - parsed);
                       this->position -= parsed;

                       ssize_t n_read = recv(this->socket, buffer + this->position, BUF_SIZE - this->position, 0);
                       if (n_read <= 0) {
                           if ((errno == EWOULDBLOCK || errno == EAGAIN) && n_read < 0 && running.load()) {
                               return true;
                           } else {
                               return false;
                           }
                       }

                       this->position += n_read;
                   }
                   std::memmove(buffer, buffer + parsed, this->position - parsed);
                   this->position -= parsed;

                   this->cmd = this->parser.Build(this->body_size);
                   this->body_size += 2;
                   this->parser.Reset();

                   this->body.clear();
                   this->state = State::Body;
               }
               // read body
               if (this->state == State::Body) {
                   if (this->body_size > 2) {
                       while (this->body_size > this->position) {
                           this->body.append(buffer, this->position);
                           this->body_size -= this->position;
                           this->position = 0;


                           ssize_t n_read = recv(this->socket, buffer, BUF_SIZE, 0);
                           if (n_read <= 0) {
                               if ((errno == EWOULDBLOCK || errno == EAGAIN) && n_read < 0 && running.load()) {
                                   return true;
                               } else {
                                   return false;
                               }
                           }

                           this->position = n_read;
                       }

                       this->body.append(buffer, this->body_size);
                       std::memmove(buffer, buffer + this->body_size, this->position - this->body_size);
                       this->position -= this->body_size;

                       this->body = this->body.substr(0, this->body.length() - 2);
                   }

                   this->cmd->Execute(*pStorage, this->body, this->out);
                   this->out.append("\r\n");
                   this->state = State::Write;
               }
           } catch (std::runtime_error &e) {
               std::string err = std::string("SERVER_ERROR ") + e.what() + std::string("\r\n");
               std::cout << err << std::endl;
               this->parser.Reset();
               return false;
               //this->state = State::Write;
           }
           if (this->state == State::Write) {
               if (this->out.size() > 2) {
                   while (this->bytes_sent_total < this->out.size()) {

                       ssize_t n_sent = send(socket, this->out.data() + this->bytes_sent_total, this->out.size() - this->bytes_sent_total, 0);
                       if (n_sent < 0) {
                            if ((errno == EWOULDBLOCK || errno == EAGAIN) && running.load()) {
                                return true;
                            } else {
                                return false;
                            }
                        }

                       this->bytes_sent_total += n_sent;
                   }
               }
               this->bytes_sent_total = 0;
               this->state = State::Read;
           }
        }
        return false;
    }
};

class Worker {
public:
    Worker(std::shared_ptr<Afina::Storage> ps);
    ~Worker();
    Worker(const Worker& w) : pStorage(w.pStorage) {};

    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(int server_socket);

    /**
     * Signal background thread to stop. After that signal thread must stop to
     * accept new connections and must stop read new commands from existing. Once
     * all readed commands are executed and results are send back to client, thread
     * must stop
     */
    void Stop();

    /**
     * Blocks calling thread until background one for this worker is actually
     * been destoryed
     */
    void Join();


    pthread_t thread;

protected:
    /**
     * Method executing by background thread
     */
    void OnRun(int server_socket);

private:
    using OnRunProxyArgs = std::pair<Worker*, int>;

    //bool Proc(Connection* conn);
    static void* OnRunProxy(void* args);
    void EraseConnection(int client_socket);

    std::vector<std::unique_ptr<Connection>> connections;
    std::shared_ptr<Afina::Storage> pStorage;
    int epfd;
    std::atomic<bool> running;
    int server_socket;


};

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
