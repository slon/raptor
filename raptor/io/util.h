#pragma once

#include <raptor/core/time.h>

namespace raptor {

class io_buff_t;

void write_all(int fd, char const* data, size_t size, duration_t* timeout);
void write_all(int fd, io_buff_t const* buf, duration_t* timeout);

void read_all(int fd, char* buff, size_t size, duration_t* timeout);

} // namespace raptor
