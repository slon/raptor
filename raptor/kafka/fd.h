#pragma once

#include <algorithm>

namespace raptor { namespace io_kafka {

class fd_t {
public:
    fd_t() : fd_(-1) {}
    explicit fd_t(int fd) : fd_(fd) {}

    ~fd_t() { close(); }

    int fd() const {
        return fd_;
    }

	fd_t dup();

    void close();

    bool is_closed() const {
        return fd_ == -1;
    }

    fd_t(const fd_t& fd) = delete;
    fd_t& operator = (const fd_t& fd) = delete;

    fd_t(fd_t&& fd) {
        fd_ = fd.fd_;
        fd.fd_ = -1;
    }

    fd_t& operator = (fd_t&& fd) {
        std::swap(fd_, fd.fd_);
        return *this;
    }

private:
    int fd_;
};

}} // namespace raptor::io_kafka
