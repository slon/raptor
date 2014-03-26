#pragma once

#include <raptor/kafka/defs.h>
#include <raptor/kafka/message_set.h>
#include <raptor/core/future.h>

namespace raptor { namespace kafka {

class kafka_client_t;

class producer_t {
public:
	producer_t(const std::string& topic, kafka_client_t* client, size_t buffer_size = 64 * 1024);

	void produce(partition_id_t partition, const std::string& message);
	void produce(partition_id_t partition, const message_t& message);	

	void flush();
private:	
	std::string topic_;
	kafka_client_t* client_;
	size_t buffer_size_;

	struct partition_t {
		message_set_builder_t builder;
		future_t<void> pending_request;
	};

	std::vector<partition_t> partitions_;

	void expand(partition_id_t partition_id);
};

}} // namespace raptor::kafka
