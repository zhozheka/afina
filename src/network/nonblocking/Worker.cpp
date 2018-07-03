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
namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps): pStorage(ps) {}

// See Worker.h
Worker::~Worker() {
}

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

bool Worker::Read(Connection* conn)
{
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    char buf[BUF_SIZE];
    Protocol::Parser parser;
    int write_socket, read_socket;
    read_socket = conn->fd;
    write_socket = conn->fd;
    while (running.load())
    {
        size_t parsed = 0;
        if (conn->state == State::kReading)
        {
            int n_read = 0;
            bool isParsed = true;
            try
            {
                while (!parser.Parse(conn->read_str, parsed))
                {
                    isParsed = false;
                    n_read = read(read_socket, buf, BUF_SIZE);
                    if (n_read > 0)
                    {
                        conn->read_str.append(buf, n_read);
                        isParsed = true;
                        parser.Reset();
                    } else {
                        break;
                    }
                }
            } catch (std::runtime_error &ex) {
                std::cout << ex.what() << '\n';
                conn->write_str = std::string("WORKER_CONNECTION_ERROR ") + ex.what() + "\r\n";
                conn->read_str.clear();
                conn->state = State::kWriting;
                continue;
            }

            if (n_read < 0)
            {
                if (!((errno == EWOULDBLOCK || errno == EAGAIN) && running.load())) {
                    return true;
                }
            }

            if (isParsed)
            {
                conn->read_str.erase(0, parsed);
                conn->state = State::kBuilding;
            } else {
                parser.Reset();
            }
        }

        if (conn->state == State::kBuilding)
        {
            uint32_t body_size = 0;
            auto command = parser.Build(body_size);
            if (conn->read_str.size() >= body_size)
            {
                std::string str_command(conn->read_str, 0, body_size);
                conn->read_str.erase(0, body_size);
                if (conn->read_str[0] == '\r')
                {
                    conn->read_str.erase(0, 2);
                }
                try {
                    command->Execute(*pStorage, str_command, conn->write_str);
                    conn->write_str += "\r\n";
                } catch (std::runtime_error &ex) {
                    conn->write_str = std::string("WORKER_CONNECTION_ERROR ") + ex.what() + "\r\n";
                    conn->read_str.clear();
                }
                parser.Reset();
                conn->state = State::kWriting;
            } else {
                conn->state = State::kReading;
            }
        }

        if (conn->state == State::kWriting)
        {
            size_t new_chunk_size = 0;
            int n_write = 1;
            while (new_chunk_size < CHUNK_SIZE)
            {
                n_write = write(write_socket, conn->write_str.c_str(), conn->write_str.size());

                if (n_write > 0) {
                    new_chunk_size += n_write;
                    conn->write_str.erase(0, n_write);
                } else {
                    break;
                }
            }

            if (n_write < 0)
            {
                if (!((errno == EWOULDBLOCK || errno == EAGAIN) && running.load())) {
                    return true;
                }
            }

            if (conn->write_str.size() == 0)
            {
                conn->state = State::kReading;
                if (conn->read_str.size() == 0)
                {
                    return false;
                }
            }
        }
    }
    return false;
}

void Worker::EraseConnection(int client_socket)
{
    for (auto it = connections.begin(); it != connections.end(); it++)
    {
        if ((*it)->fd == client_socket)
        {
            connections.erase(it);
            break;
        }
    }
}

// See Worker.h
void Worker::OnRun(int _server_socket)
{
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // TODO: implementation here
    // 1. Create epoll_context here
    // 2. Add server_socket to context
    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.

    server_socket = _server_socket;

    if ((epfd = epoll_create(EPOLL_MAX_EVENTS)) < 0)
    {
        throw std::runtime_error("Worker failed to create epoll file descriptor");
    }

    epoll_event event, events_buffer[EPOLL_MAX_EVENTS];

    Connection* server_con = new Connection(server_socket);
    event.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLHUP | EPOLLERR;
    event.data.ptr = server_con;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        throw std::runtime_error("Server epoll_ctl() failed");
    }



    while (running.load())
    {
        int n = epoll_wait(epfd, events_buffer, EPOLL_MAX_EVENTS, -1);
        if (n == -1)
        {
            throw std::runtime_error("Worker epoll_wait() failed");
        }

        for (int i = 0; i < n; ++i)
        {
            Connection* connection = reinterpret_cast<Connection*>(events_buffer[i].data.ptr);
            int client_socket = 0;
            if (connection->fd == server_socket)
            {
                client_socket = accept(server_socket, NULL, NULL);
                if (client_socket == -1) {
                    if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
                        close(server_socket);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, server_socket, NULL);
                        if (running.load())
                        {
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
            } else {
                client_socket = connection->fd;
                if (events_buffer[i].events & (EPOLLERR | EPOLLHUP))
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, NULL);
                    EraseConnection(client_socket);
                } else if (events_buffer[i].events & (EPOLLIN | EPOLLOUT)) {
                    if (!Read(connection))
                    {
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
    for (auto it = connections.begin(); it != connections.end(); it++)
    {
        epoll_ctl(epfd, EPOLL_CTL_DEL, (*it)->fd, NULL);
    }
    connections.clear();
    close(epfd);
}


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
