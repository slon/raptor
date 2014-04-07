#include <raptor/io/socket.h>

#include <system_error>

#include <raptor/core/syscall.h>

namespace raptor {

fd_guard_t socket_connect(inet_address_t address, duration_t* timeout) {
	fd_guard_t sock(socket(address.ss.ss_family, SOCK_STREAM, 0));

	if(sock.fd() == -1) {
		throw std::system_error(errno, std::system_category(), "socket(): ");
	}

	rt_ctl_nonblock(sock.fd());

	if(rt_connect(sock.fd(), address.addr(), address.addrlen(), timeout) == -1) {
		throw std::system_error(errno, std::system_category(), "connect(): ");
	}


	return sock;
}

void write_all(int fd, char const* data, size_t size, duration_t* timeout) {
	while(size != 0) {
		ssize_t res = rt_write(fd, data, size, timeout);

		if(res < 0)
			throw std::system_error(errno, std::system_category(), "rt_write: ");

		data += res;
		size -= res;
	}
}

} // namespace raptor
