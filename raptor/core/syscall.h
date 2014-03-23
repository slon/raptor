#pragma once

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

#include <raptor/core/time.h>

namespace raptor {

void rt_sleep(duration_t* timeout);

int rt_ctl_nonblock(int fd);

ssize_t rt_read(int fd, void *buf, size_t len, duration_t* timeout);
ssize_t rt_readv(int fd, struct iovec const *vec, int count, duration_t *timeout);

ssize_t rt_write(int fd, void const *buf, size_t len, duration_t *timeout);
ssize_t rt_writev(int fd, struct iovec const *vec, int count, duration_t *timeout);

ssize_t rt_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen, duration_t *timeout);
ssize_t rt_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen, duration_t *timeout);

int rt_connect(int fd, struct sockaddr const *addr, socklen_t addrlen, duration_t *timeout);
int rt_accept(int fd, struct sockaddr *addr, socklen_t *addrlen, duration_t *timeout);

} // namespace raptor
