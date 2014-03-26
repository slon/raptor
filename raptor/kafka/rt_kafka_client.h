#pragma once

#include <map>
#include <memory>

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
	rt_kafka_client_t(scheduler_t* scheduler, const options_t& options = default_options());
	~rt_kafka_client_t() { shutdown(); }

	void shutdown();

	virtual std::shared_ptr<producer_t> make_producer(const std::string& topic);

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

	void add_broker(const std::string& hostname, uint16_t port) { network->add_broker(hostname, port); }

private:
	options_t options;

	std::unique_ptr<network_t> network;

	future_t<void> send(
		const std::string& topic, partition_id_t partition,
		request_ptr_t request, response_ptr_t response
	);
};

}} // namespace raptor::kafka
