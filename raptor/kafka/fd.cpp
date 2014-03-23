#include <raptor/kafka/fd.h>

#include <unistd.h>

namespace raptor {

void fd_t::close() {
    if(fd_ != -1) ::close(fd_);
    fd_ = -1;
}

} // namespace raptor
