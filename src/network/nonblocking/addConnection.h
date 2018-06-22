#ifndef AFINA_ADDCONNECTION_H
#define AFINA_ADDCONNECTION_H

#include <afina/Storage.h>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>


namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {
class addConnection {
public:
    enum class State {
        // Connection is fully operational, tasks could be added and get executed
                kRun,

        // Connection is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
                kStopping,

        // Connection is stopped
                kStopped
    };
    addConnection();
    addConnection(std::shared_ptr<Afina::Storage> ps, int sock);
    ~addConnection();
    void routine();
    State cState = State::kRun;
private:
    std::shared_ptr<Afina::Storage> pStorage;
    Protocol::Parser parser;
    int socket;
    bool is_parsed = false;
};

}
}
}
#endif //AFINA_ADDCONNECTION_H
