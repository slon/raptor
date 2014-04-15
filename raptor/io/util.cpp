#include <raptor/io/socket.h>

#include <system_error>

#include <raptor/core/syscall.h>

namespace raptor {

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
