#pragma once

#include <raptor/core/periodic.h>
#include <raptor/server/tcp_server.h>

namespace raptor {

std::string get_graphite_prefix();
void replace(std::string* str, char from, char to);

class graphite_reporter_t {
public:
	graphite_reporter_t(scheduler_ptr_t scheduler);

	~graphite_reporter_t() { shutdown(); }

	void send_metrics();

	void shutdown() { periodic_.shutdown(); }

private:
	periodic_t periodic_;
};

struct graphite_handler_t : public tcp_handler_t {
	virtual void on_accept(int fd);
};

} // namespace raptor
