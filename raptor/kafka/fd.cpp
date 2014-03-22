#include <phantom/io_kafka/fd.h>

#include <pd/base/log.H>
#include <pd/base/exception.H>
#include <pd/bq/bq_util.H>

#include <phantom/pd.H>

#include <unistd.h>

namespace phantom { namespace io_kafka {

void fd_t::close() {
    if(fd_ != -1) ::close(fd_);
    fd_ = -1;
}

fd_t fd_t::dup() {
	fd_t dup_fd(::dup(fd_));

	if(dup_fd.fd_ == -1) {
		throw exception_sys_t(log::error, errno, "fd_t::dup(): %m");
	}

	bq_fd_setup(dup_fd.fd_);

	return std::move(dup_fd);
}


}} // namespace phantom::io_kafka
