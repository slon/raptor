#include <raptor/kafka/producer.h>

#include <raptor/kafka/kafka_client.h>

namespace raptor { namespace kafka {

producer_t::producer_t(const std::string& topic, kafka_client_t* client, const options_t& options) :
		options_(options), topic_(topic), client_(client) {}


void producer_t::produce(partition_id_t partition, const std::string& message) {
	produce(partition, message_t(message));
}

void producer_t::produce(partition_id_t partition, const message_t& message) {
	expand(partition);

	if(!builders_[partition].append(message)) {
		flush(partition);

		builders_[partition].append(message);
	}
}

void producer_t::flush() {
	for(size_t partition = 0; partition < builders_.size(); ++partition) {
		flush(partition);
	}

	while(!outstanding_requests_.empty()) {
		future_t<void> request = outstanding_requests_.front();
		outstanding_requests_.pop_front();
		request.get();
	}
}

void producer_t::flush(partition_id_t partition) {
	if(!builders_[partition].empty()) {
		future_t<void> request = client_->produce(topic_, partition, builders_[partition].build());
		builders_[partition].reset();

		outstanding_requests_.push_back(request);
		if(outstanding_requests_.size() > options_.lib.producer_max_outstanding_requests) {
			future_t<void> earliest_request = outstanding_requests_.front();
			outstanding_requests_.pop_front();
			earliest_request.get();
		}
	}
}

void producer_t::expand(partition_id_t max_partition) {
	for(partition_id_t i = builders_.size(); i <= max_partition; ++i) {
		builders_.emplace_back(options_.lib.producer_buffer_size);
	}
}

}} // namespace raptor::kafka
