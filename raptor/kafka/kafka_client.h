#pragma once

#include <string>

#include <pm/metrics.h>

#include <raptor/core/future.h>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/message_set.h>
#include <raptor/kafka/response.h>
#include <raptor/kafka/request.h>
#include <raptor/kafka/kafka_client.h>
#include <raptor/kafka/kafka_cluster.h>
#include <raptor/kafka/options.h>

namespace raptor { namespace kafka {

class kafka_client_t : public std::enable_shared_from_this<kafka_client_t> {
public:
	virtual future_t<offset_t> get_log_end_offset(
		const std::string& topic, partition_id_t partition
	) = 0;

	virtual future_t<offset_t> get_log_start_offset(
		const std::string& topic, partition_id_t partition
	) = 0;

	virtual future_t<message_set_t> fetch(
		const std::string& topic, partition_id_t partition, offset_t offset
	) = 0;

	virtual future_t<void> produce(
		const std::string& topic, partition_id_t partition, message_set_t msg_set
	) = 0;

	virtual void shutdown() = 0;
};

typedef std::shared_ptr<kafka_client_t> kafka_client_ptr_t;

class rt_kafka_client_t : public kafka_client_t {
public:	
	rt_kafka_client_t(kafka_cluster_ptr_t cluster, const options_t& options);

	future_t<offset_t> get_log_offset(const std::string& topic, partition_id_t partition, int64_t time);

	virtual future_t<offset_t> get_log_end_offset(
		const std::string& topic, partition_id_t partition
	);

	virtual future_t<offset_t> get_log_start_offset(
		const std::string& topic, partition_id_t partition
	);

	virtual future_t<message_set_t> fetch(
		const std::string& topic, partition_id_t partition, offset_t offset
	);

	virtual future_t<void> produce(
		const std::string& topic, partition_id_t partition, message_set_t message_set
	);

	virtual void shutdown();

private:
	kafka_cluster_ptr_t cluster_;

	const options_t options_;
	pm::timer_t rpc_timer_;
	pm::meter_t network_error_meter_, server_error_meter_;

	void check_response(char const* name, topic_request_ptr_t request, topic_response_ptr_t response, future_t<void> request_completed);
};

kafka_client_ptr_t make_kafka_client(
	scheduler_ptr_t scheduler,
	const broker_list_t& brokers,
	const options_t& options
);

}} // namespace raptor::kafka
