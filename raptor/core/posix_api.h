#pragma once

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace raptor { namespace internal {

class posix_api_t {
public:
	virtual int open(const char* path, int oflag) = 0;
	virtual int open(const char* path, int oflag, mode_t mode) = 0;

	virtual int fstat(int fd, struct stat *buf) = 0;

	virtual ssize_t read(int fd, void* buf, size_t nbytes) = 0;
	virtual ssize_t readv(int fd, struct iovec const* vec, int count) = 0;

	virtual ssize_t write(int fd, const void* buf, size_t nbytes) = 0;
	virtual ssize_t writev(int fd, struct iovec const* vec, int count) = 0;

	virtual ssize_t recvfrom(int fd, void* buf, size_t len, struct sockaddr* addr, socklen_t* addrlen) = 0;
	virtual ssize_t sendto(int fd, const void* buf, size_t len, struct sockaddr const* dest_addr, socklen_t addrlen) = 0;

	virtual ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count) = 0;

	virtual int close(int fd) = 0;

	virtual ~posix_api_t() {}
};

}} // namespace raptor::internal
