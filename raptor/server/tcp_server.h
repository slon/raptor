#pragma once

#include <memory>
#include <atomic>

#include <raptor/core/scheduler.h>
#include <raptor/io/fd_guard.h>

namespace raptor {

struct tcp_handler_t {
	virtual void on_accept(int socket) = 0;

	virtual ~tcp_handler_t() {};
};

fd_guard_t bind(uint16_t port);

class tcp_server_t {
public:
	struct config_t {
		config_t() : shutdown_poll_interval(0.2) {}

		duration_t shutdown_poll_interval;
	};

	tcp_server_t(scheduler_t* scheduler, std::shared_ptr<tcp_handler_t> handler, uint16_t port, config_t config = config_t());

	~tcp_server_t() { shutdown(); }

	void shutdown();

private:
	void accept_loop();

	void handle_accept(int fd);

	scheduler_t* scheduler_;
	std::shared_ptr<tcp_handler_t> handler_;
	const config_t config_;

	std::atomic<bool> shutdown_;

	fd_guard_t accept_socket_;
	fiber_t accept_fiber_;
};

} // namespace raptor
