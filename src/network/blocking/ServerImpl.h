#ifndef AFINA_NETWORK_BLOCKING_SERVER_H
#define AFINA_NETWORK_BLOCKING_SERVER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <pthread.h>
#include <unordered_set>

#include <afina/network/Server.h>
#include <afina/execute/Command.h>
#include "../../protocol/Parser.h"
#include <afina/Executor.h>
#include <sstream>

namespace Afina {
namespace Network {
namespace Blocking {

/**
 * # Network resource manager implementation
 * Server that is spawning a separate thread for each connection
 */
class ServerImpl : public Server {
public:
    ServerImpl(std::shared_ptr<Afina::Storage> ps);
    ~ServerImpl();

    // See Server.h
    void Start(uint16_t port, uint16_t workers) override;

    // See Server.h
    void Stop() override;

    // See Server.h
    void Join() override;

protected:
    /**
     * Method is running in the connection acceptor thread
     */
    void RunAcceptor();

    /**
     * Methos is running for each connection
     */
    void RunConnection(int client_socket);

    //void CleanParsed(char* buf, size_t& parsed, ssize_t& bufsize);

private:
    int server_socket;

    const ssize_t BUF_SIZE = 1024;
    static void *RunAcceptorProxy(void *p);
    static void *RunConnectionProxy(void *p);

    std::string GetVal(ssize_t body_size, int &client_socket);

    void CleanParsed(char* buf, size_t& parsed, ssize_t& readed);

    // Atomic flag to notify threads when it is time to stop. Note that
    // flag must be atomic in order to safely publish changes cross thread
    // bounds
    std::atomic<bool> running;

    // Thread that is accepting new connections
    pthread_t accept_thread;

    // Maximum number of client allowed to exists concurrently
    // on server, permits access only from inside of accept_thread.
    // Read-only
    uint16_t max_workers;
    uint16_t n_connections;
    // Port to listen for new connections, permits access only from
    // inside of accept_thread
    // Read-only
    uint16_t listen_port;

    // Mutex used to access connections list
    std::mutex connections_mutex;

    // Conditional variable used to notify waiters about empty
    // connections list
    std::condition_variable connections_cv;

    // Threads that are processing connection data, permits
    // access only from inside of accept_threadz
    std::unordered_set<pthread_t> connections;

    Executor executor;
};

} // namespace Blocking
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_BLOCKING_SERVER_H
