#pragma once

#include <algorithm>

namespace raptor {

class fd_guard_t {
public:
    fd_guard_t() : fd_(-1) {}
    explicit fd_guard_t(int fd) : fd_(fd) {}

    ~fd_guard_t() { close(); }

    int fd() const {
        return fd_;
    }

    void close();

    bool is_closed() const {
        return fd_ == -1;
    }

    fd_guard_t(const fd_guard_t& fd) = delete;
    fd_guard_t& operator = (const fd_guard_t& fd) = delete;

    fd_guard_t(fd_guard_t&& fd) {
        fd_ = fd.fd_;
        fd.fd_ = -1;
    }

    fd_guard_t& operator = (fd_guard_t&& fd) {
        std::swap(fd_, fd.fd_);
        return *this;
    }

private:
    int fd_;
};

} // namespace raptor
