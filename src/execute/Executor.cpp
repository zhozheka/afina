#include "../../include/afina/Executor.h"
#include <iostream>
#include <functional>

namespace Afina {

Executor::Executor(std::string name, size_t _low_watermark, size_t _high_watermark, size_t _max_queue_size, std::chrono::milliseconds _idle_time)
:low_watermark(_low_watermark),  high_watermark(_high_watermark), max_queue_size(_max_queue_size), idle_time(_idle_time), state(State::kRun)
{
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    std::lock_guard<std::mutex> lock(mutex);
    for (int i = 0; i < low_watermark; i++)
    {
        threads.emplace_back(perform, this);
    }
}

void Executor::Stop(bool await) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (state == State::kRun)
        {
            state = State::kStopping;
        }
    }

    empty_condition.notify_all();
    if (await) {
        for (size_t i = 0; i < threads.size(); i++) {
            if (threads[i].joinable())
            {
                threads[i].join();
            }
        }
        state = State::kStopped;
    }
}

Executor::~Executor() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    Stop(true);
}

void perform(Executor *executor) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    std::function<void()> task;

    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(executor->mutex);

            while (executor->state == Executor::State::kRun && executor->tasks.empty())
            {
                executor->empty_condition.wait(lock);
            }

            if (executor->empty_condition.wait_for(lock, executor->idle_time, [&executor]() {return (executor->tasks.empty() || executor->state == Executor::State::kStopping);}))
            {
                if (executor->threads.size() > executor->low_watermark)
                {
                    for (size_t i = 0; i < executor->threads.size(); i++)
                    {
                        if (executor->threads[i].get_id() == std::this_thread::get_id())
                        {
                            executor->threads[i].detach();
                            executor->threads.erase(executor->threads.begin() + i);
                            break;
                        }
                    }
                }
                return;
            }

            if (executor->state == Executor::State::kStopped)
            {
                return;
            }

            task = executor->tasks.front();
            executor->tasks.pop_front();
        }
        task();
    }
}

} // namespace Afina
