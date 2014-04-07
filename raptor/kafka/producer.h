#pragma once

#include <vector>
#include <string>
#include <deque>

#include <raptor/core/future.h>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/message_set.h>
#include <raptor/kafka/options.h>

namespace raptor { namespace kafka {

class kafka_client_t;

class producer_t {
public:
	producer_t(const std::string& topic, kafka_client_t* client, const options_t& options);

	void produce(partition_id_t partition, const std::string& message);
	void produce(partition_id_t partition, const message_t& message);	

	void flush();
private:	
	options_t options_;

	std::string topic_;
	kafka_client_t* client_;

	std::vector<message_set_builder_t> builders_;

	std::deque<future_t<void>> outstanding_requests_;

	void expand(partition_id_t max_partition);
	void flush(partition_id_t partition);
};

}} // namespace raptor::kafka
