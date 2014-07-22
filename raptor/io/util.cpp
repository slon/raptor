#include <raptor/io/util.h>

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

void read_all(int fd, char* buff, size_t size, duration_t* timeout) {
	size_t bytes_read = 0;
	while(bytes_read < size) {
		ssize_t n = rt_read(fd, buff + bytes_read, size - bytes_read, timeout);

		if(n == 0)
			throw std::runtime_error("rt_read: connection closed");

		if(n < 0)
			throw std::system_error(errno, std::system_category(), "rt_read: ");

		bytes_read += n;
	}
}


} // namespace raptor
