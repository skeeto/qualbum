#include "test_main.hpp"

#include "thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using qualbum::tp::ThreadPool;

TEST_CASE("thread_pool: dispatches all submitted tasks") {
    ThreadPool pool(4);
    std::atomic<int> done{0};
    constexpr int N = 200;
    for (int i = 0; i < N; ++i) {
        pool.submit([&done] {
            done.fetch_add(1, std::memory_order_relaxed);
        });
    }
    pool.wait_idle();
    REQUIRE_EQ(done.load(), N);
}

TEST_CASE("thread_pool: independent tasks see parallelism") {
    ThreadPool pool(4);
    std::atomic<int> peak{0};
    std::atomic<int> live{0};
    constexpr int N = 16;
    for (int i = 0; i < N; ++i) {
        pool.submit([&peak, &live] {
            int now = live.fetch_add(1, std::memory_order_acq_rel) + 1;
            int p = peak.load(std::memory_order_relaxed);
            while (now > p && !peak.compare_exchange_weak(p, now)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            live.fetch_sub(1, std::memory_order_acq_rel);
        });
    }
    pool.wait_idle();
    REQUIRE(peak.load() >= 2);
}

TEST_CASE("thread_pool: task exception does not kill the worker") {
    ThreadPool pool(2);
    std::atomic<int> after{0};
    pool.submit([] { throw std::runtime_error("boom"); });
    pool.submit([&after] { after.fetch_add(1, std::memory_order_relaxed); });
    pool.wait_idle();
    REQUIRE_EQ(after.load(), 1);
}
