#include <raptor/kafka/producer.h>

#include <raptor/kafka/kafka_client.h>

namespace raptor { namespace kafka {

producer_t::producer_t(const std::string& topic, kafka_client_t* client, size_t buffer_size) :
		topic_(topic), client_(client), buffer_size_(buffer_size) {}


void producer_t::produce(partition_id_t partition, const std::string& message) {
	produce(partition, message_t(message));
}

void producer_t::produce(partition_id_t partition, const message_t& message) {
	throw std::runtime_error("not implemented");
}

void producer_t::flush() {
	throw std::runtime_error("not implemented");
}

void producer_t::expand(partition_id_t partition_id) {

}

}} // namespace raptor::kafka
