#include <raptor/core/scheduler.h>
#include <raptor/kafka/rt_kafka_client.h>

#include <gtest/gtest.h>

using namespace raptor;
using namespace raptor::kafka;

TEST(kafka_test_t, shutdown) {
	auto s = make_scheduler();
	options_t opts;

	auto client = std::make_shared<rt_kafka_client_t>(s.get(), parse_broker_list("localhost:19341"), opts);
	auto offset = client->get_log_start_offset("test", 0);

	client->shutdown();
	s->shutdown();
}
