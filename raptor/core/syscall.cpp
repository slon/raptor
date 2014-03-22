#include <raptor/core/syscall.h>

#include <sys/ioctl.h>

#include <raptor/core/impl.h>

namespace raptor {

void rt_sleep(duration_t* timeout) {
	SCHEDULER_IMPL->wait_timeout(timeout);
}

int rt_ctl_noblock(int fd) {
	int i = 1;
	return ioctl(fd, FIONBIO, &i);
}

template<class ret_t, class... args_t>
inline ret_t wrap_syscall(ret_t (*fn)(int fd, args_t...), duration_t* timeout, int flag, int fd, args_t... args) {
	while(true) {
		ret_t res = (*fn)(fd, args...);

		if(res < 0 && errno == EAGAIN) {
			int wait_res = SCHEDULER_IMPL->wait_io(fd, flag, timeout);
			if(wait_res == scheduler_impl_t::TIMEDOUT) {
				errno = ETIMEDOUT;
			} else {
				continue;
			}
		}

		return res;
	}
}

ssize_t rt_read(int fd, void *buf, size_t len, duration_t* timeout) {
	return wrap_syscall(&read, timeout, EV_READ, fd, buf, len);
}

ssize_t rt_readv(int fd, struct iovec const *vec, int count, duration_t *timeout) {
	return wrap_syscall(&readv, timeout, EV_READ, fd, vec, count);
}

ssize_t rt_write(int fd, void const *buf, size_t len, duration_t *timeout) {
	return wrap_syscall(&write, timeout, EV_WRITE, fd, buf, len);
}

ssize_t rt_writev(int fd, struct iovec const *vec, int count, duration_t *timeout) {
	return wrap_syscall(&writev, timeout, EV_WRITE, fd, vec, count);
}

ssize_t rt_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen, duration_t *timeout) {
	return wrap_syscall(&recvfrom, timeout, EV_READ, fd, buf, len, flags, addr, addrlen);
}

ssize_t rt_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen, duration_t *timeout) {
	return wrap_syscall(&sendto, timeout, EV_WRITE, fd, buf, len, flags, dest_addr, addrlen);
}

int rt_accept(int fd, struct sockaddr *addr, socklen_t *addrlen, duration_t *timeout) {
	return wrap_syscall(&accept, timeout, EV_READ, fd, addr, addrlen);
}

int rt_connect(int fd, struct sockaddr const *addr, socklen_t addrlen, duration_t *timeout) {
	int res = connect(fd, addr, addrlen);

	if(res < 0 && errno == EINPROGRESS) {
		int wait_res = SCHEDULER_IMPL->wait_io(fd, EV_WRITE, timeout);

		if(wait_res == scheduler_impl_t::ERROR) {
			int err; socklen_t errlen = sizeof(err);
			getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
			errno = err;
			return -1;
		} else if(wait_res == scheduler_impl_t::TIMEDOUT) {
			errno = ETIMEDOUT;
			return -1;
		} else {
			return 0;
		}
	}

	return res;
}

} // namespace raptor
