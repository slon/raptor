#include <raptor/kafka/kafka_client.h>

#include <gmock/gmock.h>

using namespace raptor;
using namespace raptor::kafka;

class mock_kafka_client_t : public kafka_client_t {
	MOCK_METHOD2(get_log_end_offset, future_t<offset_t>(const std::string&, partition_id_t));
	MOCK_METHOD2(get_log_start_offset, future_t<offset_t>(const std::string&, partition_id_t));
	MOCK_METHOD2(fetch, future_t<message_set_t>(const std::string&, partition_id_t, offset_t));
	MOCK_METHOD2(produce, future_t<void>(const std::string&, partition_id_t, message_set_t));
};
