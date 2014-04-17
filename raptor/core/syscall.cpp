#include <raptor/core/syscall.h>

#include <sys/ioctl.h>
#include <poll.h>
#include <string.h>

#include <raptor/core/impl.h>

namespace raptor {

int wait_io(int fd, int flags, duration_t* timeout) {
	if(SCHEDULER_IMPL) {
		return SCHEDULER_IMPL->wait_io(fd, flags, timeout);
	} else {
		struct pollfd pollfd;
		memset(&pollfd, 0, sizeof(pollfd));

		pollfd.fd = fd;
		if(flags & EV_READ) pollfd.events |= POLLIN;
		if(flags & EV_WRITE) pollfd.events |= POLLOUT;

		auto poll_start = std::chrono::system_clock::now();
		int res = poll(&pollfd, 1, (int)(timeout->count() * 1000));
		auto poll_end = std::chrono::system_clock::now();

		*timeout -= (poll_end - poll_start);

		if(res == 1) {
			return scheduler_impl_t::READY;
		} else if(res == 0) {
			return scheduler_impl_t::TIMEDOUT;
		} else {
			return scheduler_impl_t::ERROR;
		}
	}
}

void rt_sleep(duration_t* timeout) {
	if(SCHEDULER_IMPL) {
		SCHEDULER_IMPL->wait_timeout(timeout);
	} else {
		usleep(timeout->count() * 1000000);
		*timeout = duration_t(0.0);
	}
}

int rt_ctl_nonblock(int fd) {
	int i = 1;
	return ioctl(fd, FIONBIO, &i);
}

template<class ret_t, class... args_t>
inline ret_t wrap_syscall(ret_t (*fn)(int fd, args_t...), duration_t* timeout, int flag, int fd, args_t... args) {
	while(true) {
		ret_t res = (*fn)(fd, args...);

		if(res < 0 && errno == EAGAIN) {
			int wait_res = wait_io(fd, flag, timeout);
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
		int wait_res = wait_io(fd, EV_WRITE, timeout);

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
