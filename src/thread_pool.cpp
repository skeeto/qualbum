#include "thread_pool.hpp"

namespace qualbum::tp {

ThreadPool::ThreadPool(unsigned threads) {
    unsigned n = threads ? threads : std::thread::hardware_concurrency();
    if (n == 0) n = 1;
    workers_.reserve(n);
    for (unsigned i = 0; i < n; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool() {
    stop();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        queue_.push(std::move(task));
        in_flight_.fetch_add(1, std::memory_order_relaxed);
    }
    cv_pop_.notify_one();
}

void ThreadPool::wait_idle() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_done_.wait(lk, [this] {
        return queue_.empty()
            && in_flight_.load(std::memory_order_relaxed) == 0;
    });
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
    }
    cv_pop_.notify_all();
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_pop_.wait(lk, [this] { return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop();
        }
        try {
            task();
        } catch (...) {
            // Swallow worker exceptions; the orchestration layer logs each
            // resize job's status, so an exception here just means the file
            // wasn't written.
        }
        if (in_flight_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lk(mu_);
            cv_done_.notify_all();
        }
    }
}

}  // namespace qualbum::tp
