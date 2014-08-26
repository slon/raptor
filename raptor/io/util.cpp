#include <raptor/io/util.h>

#include <system_error>

#include <raptor/core/syscall.h>
#include <raptor/io/io_buff.h>

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

void write_all(int fd, struct iovec* iov, int iovcnt, duration_t* timeout) {
	while(iovcnt && iov[iovcnt - 1].iov_len == 0) {
		--iovcnt;
	}

	while(iovcnt != 0) {
		ssize_t res = rt_writev(fd, iov, iovcnt, timeout);

		if(res < 0)
			throw std::system_error(errno, std::system_category(), "rt_writev: ");

		while(res != 0) {
			if((size_t)res < iov->iov_len) {
				iov->iov_len -= res;
				iov->iov_base = (char*)iov->iov_base + res;
				res = 0;
			} else {
				res -= iov->iov_len;
				iov++; --iovcnt;
			}
		}
	}
}

void write_all(int fd, io_buff_t const* buf, duration_t* timeout) {
	size_t chain_length = buf->count_chain_elements();

	struct iovec iov[chain_length];
	for(size_t i = 0; i < chain_length; ++i) {
		iov[i].iov_base = (void*)buf->data();
		iov[i].iov_len = buf->length();
		buf = buf->next();
	}

	write_all(fd, iov, chain_length, timeout);
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
