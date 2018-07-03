#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>
#include <map>
#include "Connection.cpp"

#include "Utils.h"
#define MAX_EPOLL_EVVENTS 10
#define EPOLLEXCLUSIVE 1<<28
namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps, std::shared_ptr<bool> run): pStorage(ps) {
    running = std::move(run);
}

// See Worker.h
Worker::~Worker() {
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    *running = false;
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(thread, nullptr);
}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    auto data = new std::pair<Worker,int>(*this, server_socket);
    if (pthread_create(&thread, NULL, OnRunProxy, data) < 0) {
        throw std::runtime_error("Start new thread failed Worker::Start method");
    }
}
//OnRun Proxy function
void *Worker::OnRunProxy(void *p) {
    auto data = reinterpret_cast<std::pair<Worker,int>*>(p);
    try {
        if (data->first.pStorage.get()== nullptr ) {
            std::cerr << "Errror in onrunproxy. Nullptr has been obtained.";
        }
        data->first.OnRun(&data->second);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return nullptr;
}

// See Worker.h
void* Worker::OnRun(void *args) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    struct epoll_event ev;
    struct epoll_event events[MAX_EPOLL_EVVENTS];
    int events_catched;
    //Descriptor was returned
    int epfd = epoll_create(MAX_EPOLL_EVVENTS);
    if (epfd == -1) {
        throw std::runtime_error("Epoll_create failed");
    }
    //Map with Connection and their descriptors
    std::map<int,Connection*> fd_connections;
    int socket = *reinterpret_cast<int*>(args);
    ev.data.fd = socket;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLHUP | EPOLLERR;
    //Register it
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
        throw std::runtime_error("Epoll_ctl failed");
    }
    while(*running){
        if ((events_catched = epoll_wait(epfd, events, MAX_EPOLL_EVVENTS, -1)) == -1) {
            if(errno == EINTR)
            {
                continue;
            }
            throw std::runtime_error("Epoll wait failed");
        }
        //Process all events
        for (int i = 0; i < events_catched; i++) {
            //If fd don't available for read and write or some error occured on this fd. Clear it.
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN) && !(events[i].events & EPOLLOUT))) {
                fd_connections.erase(events[i].data.fd);
                std::cerr<<"Epol error\n";
                close(events[i].data.fd);
                continue;
            } else if (socket == events[i].data.fd) {
                //Some new incoming connection
                int incoming_fd = -1;
                while (true) {
                    try {
                        incoming_fd = HandleConnection(epfd, socket);
                    }catch(std::runtime_error &err){
                        std::cout << "Error: " << err.what() << std::endl;
                    }
                    if (incoming_fd < 0){ // All connections are handled
                        break;
                    }
                    std::cout << "New conn" << std::endl;
                    auto conn = new Connection(pStorage, incoming_fd);
                    fd_connections[incoming_fd] = conn;
                    //fd_connections[incoming_fd] = Connection(pStorage, incoming_fd);
                }
                continue;
            } else {
                try {
                    if(fd_connections[events[i].data.fd]->handler() == -1) {
                        close(events[i].data.fd);
                        delete fd_connections[events[i].data.fd];
                        fd_connections.erase(events[i].data.fd);
                    }
                } catch (std::runtime_error &err){
                    continue;
                }
                continue;
            }

        }
    }
    // Server is stopping. We should proceed the last data, send users message about stopping and then close all connections
    for (auto &conn : fd_connections){
        std::cout<<"Stopping server"<<std::endl;
        conn.second->cState = Connection::StateRun::Stopping;
        conn.second->handler();
        delete conn.second;
        fd_connections.erase(conn.first);
    }

}
//Subroutine for handle new connections
int Worker::HandleConnection(int epfd, int socket){
    struct sockaddr in_addr;
    socklen_t in_len;
    int incoming_fd;
    in_len = sizeof in_addr;
    incoming_fd = accept(socket, &in_addr, &in_len);
    if (incoming_fd == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            //All connections processed
            return -1;
        } else {
            throw std::runtime_error("can't handle Handle");
        }
    }
    make_socket_non_blocking(incoming_fd);

    struct epoll_event ev;
    ev.data.fd = incoming_fd;
    ev.events = EPOLLERR | EPOLLHUP | EPOLLIN | EPOLLOUT;
    //Register it
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, incoming_fd, &ev) == -1) {
        throw std::runtime_error("Epoll_ctl error");
    }
    return incoming_fd;
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
