#include <raptor/kafka/kafka_client.h>

#include <gtest/gtest.h>

using namespace raptor;
using namespace raptor::kafka;

class kafka_client_test_t : public ::testing::Test {
public:
	options_t options;
	broker_list_t brokers;
	scheduler_ptr_t scheduler;
	kafka_client_ptr_t client;

	void SetUp() {
		scheduler = make_scheduler();
		brokers = {
			{ "localhost", 9031 },
			{ "localhost", 9032 },
			{ "localhost", 9033 }
		};
	}

	void make_client() {
		client = make_kafka_client(scheduler, brokers, options);
	}

	void TearDown() {
		if(client) client->shutdown();
		scheduler->shutdown();
	}
};

TEST_F(kafka_client_test_t, offsets) {
	make_client();

	EXPECT_EQ(0, client->get_log_start_offset("empty-topic", 0).get());
	EXPECT_EQ(0, client->get_log_end_offset("empty-topic", 0).get());
}
