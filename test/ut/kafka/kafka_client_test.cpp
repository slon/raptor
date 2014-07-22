#include <raptor/core/scheduler.h>
#include <raptor/kafka/kafka_client.h>

#include <gtest/gtest.h>

using namespace raptor;
using namespace raptor::kafka;

TEST(kafka_test_t, shutdown) {
	auto s = make_scheduler();
	options_t opts;

	auto client = make_kafka_client(s, parse_broker_list("localhost:19341"), opts);
	auto offset = client->get_log_start_offset("test", 0);

	client->shutdown();
	s->shutdown();
}
