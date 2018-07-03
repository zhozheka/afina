
#ifndef AFINA_CONNECTION_CPP
#define AFINA_CONNECTION_CPP
#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>


#include <afina/Storage.h>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>

#define BUFFER_CAPACITY 1024
namespace Afina {
namespace Network {
namespace NonBlocking {



enum State {
    Parsing, Body, Response
};

class Connection {
public:

    enum class StateRun {
        // Connection could executed
                Run,

        // Tasks are shutting down
                Stopping
    };
    Connection() {
    };
    Connection(std::shared_ptr<Afina::Storage> ps, int sock): pStorage(ps), socket(sock) {
        //Initialze parser - constructor will  be run
        parser = Protocol::Parser();
        //Make the flag is_parsed false
        is_parsed = false;
        //State should be swithced to Run
        cState = StateRun::Run;
    };
    //Destructor is simple
    ~Connection(){};
    //Handler function. Here data is processed.
    int handler() {
        bool error = false;

       while (!error && 1) {
           try {
               if (state == State::Parsing) {
                   size_t parsed = 0;
                   while (!parser.Parse(buffer, position, parsed)) {
                       std::memmove(buffer, buffer + parsed, position - parsed);
                       position -= parsed;

                       ssize_t bytes_read = recv(socket, buffer + position, BUFFER_CAPACITY - position, 0);
                       if (bytes_read <= 0) {
                           if ((errno == EWOULDBLOCK || errno == EAGAIN) && bytes_read < 0) {
                               return  0;
                           } else {
                               return -1;
                           }
                       }

                       position += bytes_read;
                   }
                   std::memmove(buffer, buffer + parsed, position - parsed);
                   position -= parsed;

                   cmd = parser.Build(body_size);
                   body_size += 2;
                   parser.Reset();

                   body.clear();
                   state = State::Body;
               }

               if (state == State::Body) {
                   if (body_size > 2) {
                       while (body_size > position) {
                           body.append(buffer, position);
                           body_size -= position;
                           position = 0;

                           ssize_t bytes_read = recv(socket, buffer, BUFFER_CAPACITY, 0);
                           if (bytes_read <= 0) {
                               if ((errno == EWOULDBLOCK || errno == EAGAIN) && bytes_read < 0) {
                                   return  0;
                               } else {
                                   return -1;
                               }
                           }

                           position = bytes_read;
                       }

                       body.append(buffer, body_size);
                       std::memmove(buffer, buffer + body_size, position - body_size);
                       position -= body_size;

                       body = body.substr(0, body.length() - 2);
                   }

                   cmd->Execute(*pStorage, body, out);
                   out.append("\r\n");
                   state = State::Response;
               }
           } catch (std::runtime_error &e) {
               out = std::string("SERVER_ERROR ") + e.what() + std::string("\r\n");
               error = true;
               state = State::Response;
           }
           if (state == State::Response) {
               if (out.size() > 2) {
                   while (bytes_sent_total < out.size()) {

                       ssize_t bytes_sent = send(socket, out.data() + bytes_sent_total, out.size() - bytes_sent_total, 0);
                       if (bytes_sent < 0) {
                           if (errno == EWOULDBLOCK || errno == EAGAIN) {
                               return 0;
                           } else {
                               return -1;
                           }
                       }

                       bytes_sent_total += bytes_sent;
                   }
               }
               bytes_sent_total = 0;
               state = State::Parsing;
           }
       }
       return 0;
    }
    StateRun cState = StateRun::Run;


private:
    std::shared_ptr<Afina::Storage> pStorage;
    Protocol::Parser parser;
    int socket;
    bool is_parsed = false;
    //Initialize buf_size 1024
    //Make the array, named buffer. Size is buf_size
    char buffer[BUFFER_CAPACITY];
    //Create out string. These string we send to user.
    std::string body;
    std::string out;
    //How many bites are parsed.
    size_t parsed = 0;
    //Current position
    size_t curr_pos = 0;
    size_t position = 0;
    //How much data we received
    ssize_t n_read = 0;
    //Body size for Parser Build
    uint32_t body_size;
    size_t bytes_sent_total = 0;
    State state = State::Parsing;
    std::unique_ptr<Execute::Command> cmd;


};


}
}
}
#endif //AFINA_CONNECTION_CPP
