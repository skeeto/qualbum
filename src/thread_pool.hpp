#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace qualbum::tp {

class ThreadPool {
public:
    explicit ThreadPool(unsigned threads = 0);   // 0 -> hardware_concurrency()
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> task);
    void wait_idle();
    void stop();
    std::size_t worker_count() const { return workers_.size(); }

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> queue_;
    std::mutex mu_;
    std::condition_variable cv_pop_;
    std::condition_variable cv_done_;
    std::atomic<std::size_t> in_flight_{0};
    bool stop_{false};
};

}  // namespace qualbum::tp
