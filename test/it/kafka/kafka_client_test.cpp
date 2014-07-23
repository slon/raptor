#include <thread>

#include <raptor/kafka/kafka_client.h>
#include <raptor/kafka/exception.h>

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

	message_set_t make_message_set(const std::vector<std::string>& msgs, compression_codec_t codec) {
		message_set_builder_t builder(64 * 1024, codec);
		for(auto& m : msgs) builder.append(m.data(), m.size());
		return builder.build();
	}

	void expect_equal(const message_set_t& msgset, const std::vector<std::string>& msgs) {
		auto iter = msgset.iter();
		int i = 0;
		while(!iter.is_end()) {
			auto m = iter.next();
			EXPECT_EQ(nullptr, m.key);
			EXPECT_EQ(msgs[i], std::string(m.value, m.value_size));
			++i;
		}

		EXPECT_EQ(i, msgs.size());
	}

	void test_produce_and_consume(const std::string& t, partition_id_t p, compression_codec_t codec) {
		offset_t old_end = client->get_log_end_offset(t, p).get();

		std::vector<std::string> msgs = {
			"foo", "bar", "", std::string(1024, 'c')
		};

		client->produce(t, p, make_message_set(msgs, codec)).get();

		std::this_thread::sleep_for(duration_t(0.05));

		EXPECT_EQ(old_end + msgs.size(), client->get_log_end_offset(t, p).get());

		auto fetched = client->fetch(t, p, old_end).get();
		expect_equal(fetched, msgs);
	}
};

TEST_F(kafka_client_test_t, offsets) {
	make_client();

	EXPECT_EQ(0, client->get_log_start_offset("empty-topic", 0).get());
	EXPECT_EQ(0, client->get_log_end_offset("empty-topic", 0).get());
}

TEST_F(kafka_client_test_t, topic_not_exists) {
	make_client();

	EXPECT_THROW(client->get_log_start_offset("topic-not-exists", 0).get(),
		unknown_topic_or_partition_t);
}

TEST_F(kafka_client_test_t, produce_and_consume) {
	make_client();

	test_produce_and_consume("test-topic-1", 0, compression_codec_t::NONE);
}

TEST_F(kafka_client_test_t, simple_compression) {
	make_client();

	test_produce_and_consume("test-topic-1", 0, compression_codec_t::SNAPPY);

	// offset_t end = client->get_log_end_offset("test-topic-1", 0).get();
	// message_set_t msgset = client->fetch("test-topic-1", 0, end - 2).get();

	// expect_equal(msgset, { "", std::string(1024, 'c') });
}
