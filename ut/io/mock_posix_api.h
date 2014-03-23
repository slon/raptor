#pragma once

#include <gmock/gmock.h>

#include <raptor/core/posix_api.h>

class mock_posix_api_t : public raptor::internal::posix_api_t {
public:
	MOCK_METHOD2(open, int(const char*, int));
	MOCK_METHOD3(open, int(const char*, int, mode_t));

	MOCK_METHOD2(fstat, int(int, struct stat*));

	MOCK_METHOD3(read, ssize_t(int, void*, size_t));
	MOCK_METHOD3(readv, ssize_t(int, struct iovec const*, int));

	MOCK_METHOD3(write, ssize_t(int, const void*, size_t));
	MOCK_METHOD3(writev, ssize_t(int, struct iovec const*, int));

	MOCK_METHOD5(test, int(int, int, int, int, int));

	MOCK_METHOD5(recvfrom, ssize_t(int, void*, size_t, struct sockaddr*, socklen_t*));
	MOCK_METHOD5(sendto, ssize_t(int, const void*, size_t, struct sockaddr const*, socklen_t));

	MOCK_METHOD4(sendfile, ssize_t(int, int, off_t*, size_t));

	MOCK_METHOD1(close, int(int));
};

