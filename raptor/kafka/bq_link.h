#pragma once

#include <queue>
#include <atomic>
#include <utility>

#include <pd/bq/bq_mutex.H>

#include <phantom/pd.H>

#include <phantom/io_kafka/future.h>
#include <phantom/io_kafka/channel.h>
#include <phantom/io_kafka/countdown.h>

#include <phantom/io_kafka/fd.h>
#include <phantom/io_kafka/wire.h>
#include <phantom/io_kafka/link.h>

namespace phantom {

class scheduler_t;

namespace io_kafka {

class request_t;
class response_t;
class options_t;

class bq_link_t : public link_t {
public:
	bq_link_t(fd_t socket, const options_t& options);
	~bq_link_t();

	void start(scheduler_t* scheduler);

	virtual future_t<void> send(request_ptr_t request, response_ptr_t response);

	virtual void close();
	virtual bool is_closed();

private:
	fd_t send_fd, recv_fd;
	const options_t& options;

	struct task_t {
		request_ptr_t request;
		response_ptr_t response;
		promise_t<void> promise;
	};

	channel_t<task_t> send_channel, recv_channel;
	countdown_t active_coroutines;

	void send_loop();
	void recv_loop();
};

}} // namespace phantom::io_kafka
