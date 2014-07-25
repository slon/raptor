#pragma once

#include <utility>

#include <raptor/core/future.h>
#include <raptor/core/scheduler.h>
#include <raptor/core/channel.h>
#include <raptor/core/mutex.h>
#include <raptor/io/fd_guard.h>

#include <raptor/kafka/request.h>
#include <raptor/kafka/response.h>
#include <raptor/kafka/options.h>
#include <raptor/kafka/metadata.h>

namespace raptor { namespace kafka {

struct kafka_rpc_t {
	request_ptr_t request;
	response_ptr_t response;
	promise_t<void> promise;
};

struct topic_kafka_rpc_t {
	topic_request_ptr_t request;
	topic_response_ptr_t response;
	promise_t<void> promise;

	kafka_rpc_t to_kafka_rpc() {
		return { request, response, promise };
	}
};

class kafka_link_t {
public:
	virtual ~kafka_link_t() {}

	virtual void shutdown() = 0;
	virtual bool is_closed() = 0;
	virtual void send(kafka_rpc_t rpc) = 0;
};

typedef std::shared_ptr<kafka_link_t> kafka_link_ptr_t;

class kafka_network_t {
public:
	virtual ~kafka_network_t() {}

	virtual void shutdown() = 0;
	virtual void send(const broker_addr_t& broker, kafka_rpc_t rpc) = 0;
};

typedef std::shared_ptr<kafka_network_t> kafka_network_ptr_t;

class kafka_cluster_t {
public:
	virtual void shutdown() = 0;
	virtual future_t<void> send(topic_request_ptr_t request, topic_response_ptr_t response) = 0;

	virtual ~kafka_cluster_t() {}
};

typedef std::shared_ptr<kafka_cluster_t> kafka_cluster_ptr_t;

class rt_kafka_link_t : public kafka_link_t {
public:
	rt_kafka_link_t(const broker_addr_t& broker, scheduler_ptr_t scheduler, options_t options);

	virtual void shutdown();
	virtual bool is_closed();
	virtual void send(kafka_rpc_t rpc);

private:
	fd_guard_t socket_;
	const options_t options_;

	channel_t<kafka_rpc_t> send_channel_, recv_channel_;

	void send_loop(broker_addr_t broker);
	void recv_loop();

	fiber_t send_fiber_, recv_fiber_;

	mutex_t closing_error_lock_;
	std::exception_ptr closing_error_;

	std::exception_ptr get_closing_error();
	void close(std::exception_ptr err);

	void connect(const broker_addr_t& broker);
};

class rt_kafka_network_t : public kafka_network_t {
public:
	rt_kafka_network_t(scheduler_ptr_t scheduler, const options_t& options);

	virtual void shutdown();

	virtual void send(const broker_addr_t& broker, kafka_rpc_t rpc);

private:
	scheduler_ptr_t scheduler_;
	const options_t options_;

	mutex_t mutex_;
	std::map<broker_addr_t, kafka_link_ptr_t> active_links_;
};

class rt_kafka_cluster_t : public kafka_cluster_t {
public:
	rt_kafka_cluster_t(
		scheduler_ptr_t scheduler,
		kafka_network_ptr_t network,
		const broker_list_t& bootstrap_brokers,
		options_t options
	);

	virtual future_t<void> send(topic_request_ptr_t request, topic_response_ptr_t response);

	virtual void shutdown();

private:
	const options_t options_;

	const broker_list_t bootstrap_brokers_;
	size_t next_broker_;

	kafka_network_ptr_t network_;

	metadata_t metadata_;
	std::atomic_bool metadata_correct_;
	time_point_t next_allowed_refresh_;

	channel_t<topic_kafka_rpc_t> rpc_queue_;

	fiber_t routing_fiber_;

	void router_loop();
	void refresh_metadata();
	void route_rpc(topic_kafka_rpc_t rpc);
	void start_metadata_refresh();

	const broker_addr_t& get_next_broker();
};

}} // namespace kafka
