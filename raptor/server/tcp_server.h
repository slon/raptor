#pragma once

#include <memory>

#include <raptor/core/scheduler.h>

namespace raptor {

struct tcp_handler_t {
	virtual void on_accept(const fd_t& socket) = 0;

	virtual ~tcp_handler_t() {};
};

class tcp_server_t {
public:
	tcp_server_t(scheduler_t scheduler, std::shared_ptr<tcp_handler_t> handler);

	void bind();

	void start();

	void stop();

private:
};

} // namespace raptor
