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
#include "addConnection.h"

#include "Utils.h"
#define MAXEVENTS 100
#define EPOLLEXCLUSIVE 1 << 28
namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps, std::shared_ptr<bool> run): pStorage(ps) {
        running = std::move(run);
        std::cout << "Init pStorage in Worker at ptr " << ps.get() << std::endl;
}

// See Worker.h
Worker::~Worker() {
}
void *Worker::OnRunProxy(void *p) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    //Worker *srv = reinterpret_cast<Worker *>(p);
    auto data = reinterpret_cast<std::pair<Worker,int>*>(p);
    try {
        if (data->first.pStorage.get()== nullptr || data->second > 20) {
            std::cerr << "In onrunproxy server socket is " << data->second << " pStorage is "
                      << data->first.pStorage.get() << "\n";
        }
        data->first.OnRun(&data->second);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return nullptr;
}
// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    //running.store(true);
    socket = server_socket;
    std::cout << "In start socket is " << socket << " pStorage is " << pStorage.get() << "\n";
    auto data = new std::pair<Worker,int>(*this, socket);
    if (pthread_create(&thread, NULL, OnRunProxy, data) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
    //std::cout << "New worker at serv_sock_descriptor " << data->second << std::endl;
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
void* Worker::OnRun(void *args) {
        std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
        struct epoll_event ev;
        struct epoll_event events[MAXEVENTS];
        int res, events_catched;
        int efd = epoll_create(0xCAFE);
        int infd = -1;
        std::cout << "Worker initialized at descriptor " << efd << std::endl;
        std::map<int,newConn> fd_conns;
        if (efd == -1) {
            throw std::runtime_error("epoll_create");
        }
        socket = *reinterpret_cast<int*>(args);
        ev.data.fd = socket;
        ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLHUP | EPOLLERR;
        res = epoll_ctl(efd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (res == -1) {
            throw std::runtime_error("epoll ctl");
        }
        std::cout << "Connected epoll " << efd <<  " to server socket " << socket << std::endl;
        long counter = 0;



        while(*running) {
            //sleep(1);
            //std::cerr << "THERE WERE " << counter << " READS ON EPOLL " << efd << std::endl;
            //std::cout << "In OnRun infinity loop pStorage is " << pStorage.get() << " efd " << efd << " socket " << socket << std::endl;
            if ((events_catched = epoll_wait(efd, events, MAXEVENTS, -1)) == -1) {
                if(errno == EINTR)
                {
                    continue;
                }
                throw std::runtime_error("epoll wait");
            }
            //std::cout << "SOME EVENTS CATCHED: "<< events_catched <<"\n";
            for (int i = 0; i < events_catched; i++) {
                if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
                        (!(events[i].events & EPOLLIN) && !(events[i].events & EPOLLOUT))) {
                    /* An error has occured on this fd, or the socket is not
                       ready for reading (why were we notified then?) */
                    std::cout << "User on socket " << events[i].data.fd << " disconnected\n";
                    fd_conns.erase(events[i].data.fd); // Clear connection.
                    fprintf(stderr, "epoll error\n");
                    close(events[i].data.fd);
                    continue;

                } else if (socket == events[i].data.fd) {
                    /* We have a notification on the listening socket, which
                       means one or more incoming connections. */
                    while (1) {
                        try {
                            infd = AcquireConn(efd, socket);
                        } catch(std::runtime_error &err){
                            std::cout << "Error: " << err.what() << std::endl;
                        }
                        if (infd < 0){ // All new connections acquired
                            break;
                        }
                        fd_conns[infd] = newConn(pStorage, infd);
                    }
                    continue;



                } else { // We've got some new data. Process it, bitch!
                    try {
                        fd_conns[events[i].data.fd].routine();
                    }catch (std::runtime_error &err){
                        std::cout << err.what() << "- error on fd " << events[i].data.fd << std::endl;
                        continue;
                    }
                    //counter++;
                    continue;
                    //close(events[i].data.fd);
                }

            }
        }
        // Server is stopping. We should proceed the last data, send users message about stopping and then close all connections
        //std::cerr << "STOPPING: THERE WERE " << counter << " READS ON EPOLL " << efd << std::endl;
        std::cout << "In OnRun infinity loop pStorage is " << pStorage.get() << " efd " << efd << " socket " << socket << std::endl;
        std::cout << "There are "<< fd_conns.size() <<" connections to be closed" << std::endl;
        for (auto &conn : fd_conns){
            std::cout << "Closing conn on fd " << conn.first << std::endl;
            conn.second.cState = newConn::State::kStopping;
            conn.second.routine();
            fd_conns.erase(conn.first);
        }

    }

int Worker::AcquireConn(int efd, int socket){
    struct sockaddr in_addr;
    socklen_t in_len;
    int infd;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    in_len = sizeof in_addr;
    infd = accept(socket, &in_addr, &in_len);
    if (infd == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            /* We have processed all incoming
               connections. */
            return -1;
        } else {
            throw std::runtime_error("Accept");
        }
    }

    int s = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
                    NI_NUMERICHOST | NI_NUMERICSERV);
    if (s == 0) {
        printf("Accepted connection on descriptor %d "
                       "(host=%s, port=%s, epoll=%d)\n",
               infd, hbuf, sbuf, efd);
    }

    /* Make the incoming socket non-blocking and add it to the
       list of fds to monitor. */
    make_socket_non_blocking(infd);
    if (s == -1)
        abort();
    struct epoll_event ev;
    ev.data.fd = infd;
    //ev.events = EPOLLIN | EPOLLET;
    ev.events = EPOLLERR | EPOLLHUP | EPOLLIN | EPOLLOUT;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &ev);
    if (s == -1) {
        throw std::runtime_error("epoll_ctl");
    }
    return infd;
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
