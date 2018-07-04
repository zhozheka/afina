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

namespace Afina {
namespace Network {
namespace NonBlocking {

bool Read(Worker::Connection* conn) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    char buf[BUF_SIZE];
    Protocol::Parser parser;
    int socket = conn->socket;
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
                    n_read = read(socket, buf, BUF_SIZE);
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
                n_write = write(socket, conn->write_str.c_str(), conn->write_str.size());

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


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
