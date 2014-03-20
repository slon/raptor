#pragma once

#include <unistd.h>
#include <errno.h>

#include <raptor/core/time.h>

namespace raptor {

int rt_read(int fd, void *buf, size_t len, duration_t* timeout);

} // namespace raptor
