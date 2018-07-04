#include "Worker.h"

#include <memory>
#include <string>
#include <stdexcept>

#include <iostream>
#include <signal.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signalfd.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../protocol/Parser.h"
#include <afina/execute/Command.h>
#include "Utils.h"

#define EPOLLEXCLUSIVE 1<<28
#define BUF_SIZE 1024
#define EPOLL_MAX_EVENTS 10

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps): pStorage(ps) {}

// See Worker.h
Worker::~Worker() {}

void* Worker::OnRunProxy(void* _args) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    auto args = reinterpret_cast<std::pair<Worker*, int>*>(_args);
    Worker* worker = args->first;
    int server_socket = args->second;
    worker->OnRun(server_socket);
    return 0;
}

// See Worker.h
void Worker::Start(int _server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    server_socket = _server_socket;
    running.store(true);
    auto args = new OnRunProxyArgs(this, server_socket);
    pthread_t buffer;
    if (pthread_create(&buffer, NULL, Afina::Network::NonBlocking::Worker::OnRunProxy, args) < 0) {
        throw std::runtime_error("Could not create worker thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    shutdown(server_socket, SHUT_RDWR);
    pthread_join(thread, NULL);
}

bool Worker::Read(Connection* conn) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    char buf[BUF_SIZE];
    Protocol::Parser parser;
    int socket = conn->socket;

    while (running.load()) {
        size_t parsed = 0;
        if (conn->state == State::Read) {
            int n_read = 0;
            bool isParsed = true;
            try {
                while (!parser.Parse(conn->readBuf, parsed)) {
                    isParsed = false;
                    n_read = read(socket, buf, BUF_SIZE);
                    if (n_read > 0) {
                        conn->readBuf.append(buf, n_read);
                        isParsed = true;
                        parser.Reset();
                    } else {
                        break;
                    }
                }
            } catch (std::runtime_error &ex) {
                std::cout << ex.what() << '\n';
                conn->outBuf = std::string("WORKER_CONNECTION_ERROR ") + ex.what() + "\r\n";
                conn->readBuf.clear();
                conn->state = State::Write;
                continue;
            }

            if (n_read < 0) {
                if (!((errno == EWOULDBLOCK || errno == EAGAIN) && running.load())) {
                    return true;
                }
            }

            if (isParsed) {
                conn->readBuf.erase(0, parsed);
                conn->state = State::Body;
            } else {
                parser.Reset();
            }
        }

        if (conn->state == State::Body) {
            uint32_t body_size = 0;
            auto command = parser.Build(body_size);
            if (conn->readBuf.size() >= body_size) {
                std::string str_command(conn->readBuf, 0, body_size);
                conn->readBuf.erase(0, body_size);
                if (conn->readBuf[0] == '\r') {
                    conn->readBuf.erase(0, 2);
                }
                try {
                    command->Execute(*pStorage, str_command, conn->outBuf);
                    conn->outBuf += "\r\n";
                } catch (std::runtime_error &ex) {
                    conn->outBuf = std::string("WORKER_CONNECTION_ERROR ") + ex.what() + "\r\n";
                    conn->readBuf.clear();
                }
                parser.Reset();
                conn->state = State::Write;
            } else {
                conn->state = State::Read;
            }
        }

        if (conn->state == State::Write) {
            size_t n_sent = 0;
            int n_write = 1;
            while (n_sent < conn->outBuf.size()) {
                n_write = write(socket, conn->outBuf.c_str(), conn->outBuf.size());

                if (n_write > 0) {
                    n_sent += n_write;
                    conn->outBuf.erase(0, n_write);
                } else {
                    break;
                }
            }

            if (n_write < 0) {
                if (!((errno == EWOULDBLOCK || errno == EAGAIN) && running.load())) {
                    return true;
                }
            }

            if (conn->outBuf.size() == 0) {
                conn->state = State::Read;
                if (conn->readBuf.size() == 0) {
                    return false;
                }
            }
        }
    }
    return false;
}

void Worker::EraseConnection(int client_socket) {
    for (auto it = connections.begin(); it != connections.end(); it++) {
        if ((*it)->socket == client_socket) {
            connections.erase(it);
            break;
        }
    }
    //for (auto &conn: connections) {
    //     if (conn->socket == client_socket) {
    //         connection.erase(conn);
    //     }
    // }
}

// See Worker.h
void Worker::OnRun(int _server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    server_socket = _server_socket;
    auto epfd = epoll_create(EPOLL_MAX_EVENTS);
    if (epfd < 0) {
        throw std::runtime_error("Worker failed to create epoll file descriptor");
    }

    epoll_event event, events_buffer[EPOLL_MAX_EVENTS];

    Connection* server_con = new Connection(server_socket);
    event.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLHUP | EPOLLERR;
    event.data.ptr = server_con;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
        throw std::runtime_error("Server epoll_ctl() failed");
    }

    while (running.load()) {
        int n_ev = epoll_wait(epfd, events_buffer, EPOLL_MAX_EVENTS, -1);
        if (n_ev == -1) {
            throw std::runtime_error("Worker epoll_wait() failed");
        }

        for (int i = 0; i < n_ev; ++i) {
            Connection* connection = reinterpret_cast<Connection*>(events_buffer[i].data.ptr);
            //new connection
            if (connection->socket == server_socket) {
                auto client_socket = accept(server_socket, NULL, NULL);
                if (client_socket == -1) {
                    if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
                        close(server_socket);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, server_socket, NULL);
                        if (running.load()) {
                            throw std::runtime_error("Worker failed to accept()");
                        }
                    }
                } else {
                    make_socket_non_blocking(client_socket);
                    event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR;
                    auto connection = new Connection(client_socket);
                    connections.emplace_back(std::move(connection));
                    event.data.ptr = connections.back().get();
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket, &event) == -1) {
                        throw std::runtime_error("Worker failed to assign client socket to epoll");
                    }
                }
            //proceed connection
            } else {
                auto client_socket = connection->socket;
                if (events_buffer[i].events & (EPOLLERR | EPOLLHUP)) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, NULL);
                    EraseConnection(client_socket);
                } else if (events_buffer[i].events & (EPOLLIN | EPOLLOUT)) {
                    if (!Read(connection)) {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, NULL);
                        EraseConnection(client_socket);
                    }
                } else {
                    EraseConnection(client_socket);
                    throw std::runtime_error("Epoll event incorrect");
                }
            }
        }
    }

    for (auto &conn: connections) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, conn->socket, NULL);
    }
    connections.clear();
    close(epfd);
}


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
