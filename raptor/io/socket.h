#pragma once

#include <raptor/core/time.h>

#include <raptor/io/fd_guard.h>
#include <raptor/io/inet_address.h>

namespace raptor {

fd_guard_t socket_connect(inet_address_t address, duration_t* timeout);

void write_all(int fd, char const* data, size_t size, duration_t* timeout = NULL);

} // namespace raptor
