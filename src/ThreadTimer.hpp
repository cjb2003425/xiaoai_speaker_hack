#ifndef THREAD_TIMER_HPP
#define THREAD_TIMER_HPP

#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <functional>

class ThreadTimer {
public:
    using TimerCallback = std::function<void(void)>;

    ThreadTimer() : tick(0), callback(nullptr), running(false) {}

    ~ThreadTimer() {
        stop();
    }

    void set(int seconds, TimerCallback cb) {
        tick.store(seconds, std::memory_order_relaxed);
        callback = std::move(cb);
    }

    void start() {
        stop(); // Ensure any existing thread is terminated before starting a new one
        running.store(true, std::memory_order_relaxed);
        timerThread = std::thread(&ThreadTimer::process, this);
    }

    void stop() {
        running.store(false, std::memory_order_relaxed);
        if (timerThread.joinable()) {
            timerThread.join();
        }
    }

private:
    void process() {
        while (running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (tick.fetch_sub(1, std::memory_order_relaxed) == 0 && callback) {
                callback();
            }
        }
    }

    std::atomic<uint32_t> tick;
    TimerCallback callback;
    std::atomic<bool> running;
    std::thread timerThread;
};

#endif // THREAD_TIMER_HPP
