#pragma once

#include <string>

#include <raptor/core/future.h>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/message_set.h>
#include <raptor/kafka/producer.h>

namespace raptor { namespace kafka {

class kafka_client_t {
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
};

}} // namespace raptor::kafka
