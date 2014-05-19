#include <raptor/core/scheduler.h>
#include <raptor/kafka/rt_kafka_client.h>

#include <gtest/gtest.h>

using namespace raptor;
using namespace raptor::kafka;

TEST(kafka_test_t, DISABLED_shutdown) {
	scheduler_t s;
	options_t opts;
	rt_kafka_client_t client(&s, parse_broker_list("localhost:19341"), opts);

	auto offset = client.get_log_start_offset("test", 0);
}
