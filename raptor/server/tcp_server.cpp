#include <raptor/server/tcp_server.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <glog/logging.h>

#include <raptor/core/syscall.h>
#include <raptor/io/inet_address.h>

namespace raptor {

tcp_server_t::tcp_server_t(scheduler_ptr_t scheduler,
			std::shared_ptr<tcp_handler_t> handler,
			uint16_t port,
			config_t config) :
		scheduler_(scheduler),
		handler_(handler),
		config_(config),
		shutdown_(false) {
	auto addr = inet_address_t::resolve_ip("localhost");
	addr.set_port(port);
	accept_socket_ = addr.bind();
	accept_fiber_ = scheduler_->start(&tcp_server_t::accept_loop, this);
}

void tcp_server_t::accept_loop() {
	while(!shutdown_) {
		inet_address_t peer_address;

		duration_t timeout = config_.shutdown_poll_interval;
		fd_guard_t sock(rt_accept(accept_socket_.fd(), peer_address.addr(), peer_address.addrlen_ptr(), &timeout));
		if(sock.fd() < 0) {
			if(errno != ETIMEDOUT) {
				PLOG(ERROR) << "accept() failed";
				break;
			} else {
				continue;
			}
		}

		rt_ctl_nonblock(sock.fd());
		active_handlers_.inc();
		scheduler_->start(&tcp_server_t::handle_accept, this, sock.release());
	}
}

void tcp_server_t::handle_accept(int fd) {
	try {
		fd_guard_t sock(fd);
		handler_->on_accept(sock.fd());
	} catch(const std::exception& e) {
		PLOG(ERROR) << e.what();
	}
	active_handlers_.dec();
}

void tcp_server_t::shutdown() {
	shutdown_ = true;
	accept_fiber_.join();
	active_handlers_.wait_zero();
}

} // namespace raptor
