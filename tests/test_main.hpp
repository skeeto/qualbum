#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace qbt {

struct TestCase {
    const char* file;
    int line;
    std::string name;
    std::function<void()> fn;
};

class Registry {
public:
    static Registry& get() {
        static Registry r;
        return r;
    }
    std::size_t add(TestCase tc) {
        cases_.push_back(std::move(tc));
        return cases_.size();
    }
    const std::vector<TestCase>& cases() const { return cases_; }

private:
    std::vector<TestCase> cases_;
};

class FailException : public std::exception {
public:
    explicit FailException(std::string m) : msg_(std::move(m)) {}
    const char* what() const noexcept override { return msg_.c_str(); }

private:
    std::string msg_;
};

struct CheckState {
    int checks{0};
    int failures{0};
};

inline CheckState& state() {
    thread_local CheckState s;
    return s;
}

}  // namespace qbt

#define QBT_CAT2(a, b) a##b
#define QBT_CAT(a, b) QBT_CAT2(a, b)

#define TEST_CASE(name_string)                                              \
    static void QBT_CAT(qbt_test_fn_, __LINE__)();                          \
    static const std::size_t QBT_CAT(qbt_test_reg_, __LINE__) =             \
        ::qbt::Registry::get().add(                                         \
            {__FILE__, __LINE__, (name_string),                             \
             &QBT_CAT(qbt_test_fn_, __LINE__)});                            \
    static void QBT_CAT(qbt_test_fn_, __LINE__)()

#define REQUIRE(expr)                                                       \
    do {                                                                    \
        ++::qbt::state().checks;                                            \
        if (!(expr)) {                                                      \
            ++::qbt::state().failures;                                      \
            std::ostringstream _qbt_oss;                                    \
            _qbt_oss << __FILE__ << ':' << __LINE__                         \
                     << " REQUIRE FAILED: " #expr;                          \
            throw ::qbt::FailException(_qbt_oss.str());                     \
        }                                                                   \
    } while (0)

#define CHECK(expr)                                                         \
    do {                                                                    \
        ++::qbt::state().checks;                                            \
        if (!(expr)) {                                                      \
            ++::qbt::state().failures;                                      \
            std::fprintf(stderr,                                            \
                         "%s:%d CHECK FAILED: %s\n",                        \
                         __FILE__, __LINE__, #expr);                        \
        }                                                                   \
    } while (0)

#define REQUIRE_EQ(a, b)                                                    \
    do {                                                                    \
        ++::qbt::state().checks;                                            \
        auto _qbt_a = (a);                                                  \
        auto _qbt_b = (b);                                                  \
        if (!(_qbt_a == _qbt_b)) {                                          \
            ++::qbt::state().failures;                                      \
            std::ostringstream _qbt_oss;                                    \
            _qbt_oss << __FILE__ << ':' << __LINE__                         \
                     << " REQUIRE_EQ FAILED: " #a " == " #b                 \
                     << "\n  lhs: " << _qbt_a                               \
                     << "\n  rhs: " << _qbt_b;                              \
            throw ::qbt::FailException(_qbt_oss.str());                     \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)                                                      \
    do {                                                                    \
        ++::qbt::state().checks;                                            \
        auto _qbt_a = (a);                                                  \
        auto _qbt_b = (b);                                                  \
        if (!(_qbt_a == _qbt_b)) {                                          \
            ++::qbt::state().failures;                                      \
            std::ostringstream _qbt_oss;                                    \
            _qbt_oss << _qbt_a;                                             \
            std::ostringstream _qbt_oss2;                                   \
            _qbt_oss2 << _qbt_b;                                            \
            std::fprintf(stderr,                                            \
                         "%s:%d CHECK_EQ FAILED: %s == %s\n  lhs: %s\n  rhs: %s\n", \
                         __FILE__, __LINE__, #a, #b,                        \
                         _qbt_oss.str().c_str(), _qbt_oss2.str().c_str());  \
        }                                                                   \
    } while (0)

#define REQUIRE_THROWS(expr)                                                \
    do {                                                                    \
        ++::qbt::state().checks;                                            \
        bool _qbt_threw = false;                                            \
        try { (void)(expr); }                                               \
        catch (...) { _qbt_threw = true; }                                  \
        if (!_qbt_threw) {                                                  \
            ++::qbt::state().failures;                                      \
            std::ostringstream _qbt_oss;                                    \
            _qbt_oss << __FILE__ << ':' << __LINE__                         \
                     << " REQUIRE_THROWS FAILED: " #expr;                   \
            throw ::qbt::FailException(_qbt_oss.str());                     \
        }                                                                   \
    } while (0)
