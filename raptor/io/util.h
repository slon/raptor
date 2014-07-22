#pragma once

#include <raptor/core/time.h>

#include <raptor/io/fd_guard.h>
#include <raptor/io/inet_address.h>

namespace raptor {

void write_all(int fd, char const* data, size_t size, duration_t* timeout);
void read_all(int fd, char* buff, size_t size, duration_t* timeout);

} // namespace raptor
