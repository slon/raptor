#include <raptor/server/tcp_server.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <glog/logging.h>

#include <raptor/core/syscall.h>

namespace raptor {

tcp_server_t::tcp_server_t(scheduler_t* scheduler,
			std::shared_ptr<tcp_handler_t> handler,
			uint16_t port,
			config_t config) :
		scheduler_(scheduler),
		handler_(handler),
		config_(config),
		shutdown_(false) {
	accept_socket_ = bind(port);
	accept_fiber_ = scheduler_->start(&tcp_server_t::accept_loop, this);
}

fd_guard_t bind(uint16_t port) {
	fd_guard_t sock(socket(AF_INET, SOCK_STREAM, 0));

	if(sock.fd() < 0)
		throw std::system_error(errno, std::system_category(), "socket(): ");

	rt_ctl_nonblock(sock.fd());

	int i = 1;
	if(setsockopt(sock.fd(), SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0)
		throw std::system_error(errno, std::system_category(), "setsockopt(): ");

	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sock.fd(), (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0)
		throw std::system_error(errno, std::system_category(), "bind(): ");

	if(listen(sock.fd(), SOMAXCONN) < 0)
		throw std::system_error(errno, std::system_category(), "listen(): ");

	return std::move(sock);
}

void tcp_server_t::accept_loop() {
	while(!shutdown_) {
		struct sockaddr_in sockaddr;
		socklen_t sockaddr_len = sizeof(sockaddr);

		duration_t timeout = config_.shutdown_poll_interval;
		fd_guard_t sock(rt_accept(accept_socket_.fd(), (struct sockaddr*)&sockaddr, &sockaddr_len, &timeout));
		if(sock.fd() < 0) {
			if(errno != ETIMEDOUT) PLOG(ERROR) << "accept failed";
			continue;
		}

		scheduler_->start(&tcp_server_t::handle_accept, this, sock.release());
	}
}

void tcp_server_t::handle_accept(int fd) {
	fd_guard_t sock(fd);
	handler_->on_accept(sock.fd());
}

void tcp_server_t::shutdown() {
	shutdown_ = true;
	accept_fiber_.join();
}

} // namespace raptor
