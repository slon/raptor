#pragma once

#include <raptor/kafka/defs.h>
#include <raptor/kafka/message_set.h>

namespace raptor { namespace kafka {

class kafka_client_t;

class producer_t {
public:
	void produce(partition_id_t partition, const std::string& message) {
		produce(partition, message_t(message));
	}

	void produce(partition_id_t partition, const message_t& message) {
		throw std::runtime_error("not implemented");
	}
	
	void flush() { throw std::runtime_error("not implemented"); }
private:	
	std::string topic_;
	kafka_client_t* client_;

	struct partition_t {
		message_set_builder_t builder;
		future_t<void> pending_request;
	};

	std::vector<partition_t> partitions_;

	void expand(partition_id_t partition_id);
};

}} // namespace raptor::kafka
