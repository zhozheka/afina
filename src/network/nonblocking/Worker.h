#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <memory>
#include <atomic>
#include <pthread.h>
#include <vector>
#include <string>
#include <unistd.h>

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
    Connection(int _socket) : socket(_socket), state(State::Read) {
        readBuf = "";
        outBuf = "";
    }
    ~Connection(void) {
        close(socket);
    }
    int socket;
    std::string readBuf;
    std::string outBuf;
    State state;
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

    bool Read(Connection* conn);
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
