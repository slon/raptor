#pragma once

#include <map>
#include <memory>

#include <pm/metrics.h>

#include <raptor/core/scheduler.h>

#include <raptor/kafka/kafka_client.h>
#include <raptor/kafka/metadata.h>
#include <raptor/kafka/response.h>
#include <raptor/kafka/request.h>
#include <raptor/kafka/options.h>
#include <raptor/kafka/network.h>

namespace raptor { namespace kafka {

class rt_kafka_client_t : public kafka_client_t {
public:
	rt_kafka_client_t(scheduler_t* scheduler, const broker_list_t& broker_list, const options_t& options = options_t());

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

private:
	options_t options;
	pm::timer_t rpc_timer;
	pm::meter_t error_meter;

	std::unique_ptr<network_t> network;

	future_t<void> send(
		const std::string& topic, partition_id_t partition,
		request_ptr_t request, response_ptr_t response
	);
};

}} // namespace raptor::kafka
