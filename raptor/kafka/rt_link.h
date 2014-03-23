#pragma once

#include <raptor/core/future.h>
#include <raptor/core/channel.h>
#include <raptor/core/scheduler.h>

#include <raptor/kafka/fd.h>
#include <raptor/kafka/wire.h>
#include <raptor/kafka/link.h>

namespace raptor {

class scheduler_t;

namespace kafka {

class request_t;
class response_t;
class options_t;

class rt_link_t : public link_t {
public:
	rt_link_t(fd_t socket, const options_t& options);
	~rt_link_t();

	void start(scheduler_t scheduler);

	virtual future_t<void> send(request_ptr_t request, response_ptr_t response);

	virtual void close();
	virtual bool is_closed();

private:
	fd_t socket;
	const options_t& options;

	struct task_t {
		request_ptr_t request;
		response_ptr_t response;
		promise_t<void> promise;
	};

	channel_t<task_t> send_channel, recv_channel;

	void send_loop();
	void recv_loop();

	fiber_t send_fiber, recv_fiber;
};

}} // namespace raptor::kafka
