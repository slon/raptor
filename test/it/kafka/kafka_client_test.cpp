#include <thread>

#include <raptor/core/syscall.h>

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

	static message_set_t make_message_set(const std::vector<std::string>& msgs, compression_codec_t codec) {
		message_set_builder_t builder(64 * 1024, codec);
		for(auto& m : msgs) builder.append(m.data(), m.size());
		return builder.build();
	}

	void expect_equal(const message_set_t& msgset, const std::vector<std::string>& msgs) {
		auto iter = msgset.iter();
		size_t i = 0;
		while(!iter.is_end() && i < msgs.size()) {
			auto m = iter.next();
			EXPECT_EQ(nullptr, m.key);
			EXPECT_EQ(msgs[i], std::string(m.value, m.value_size));
			++i;
		}

		EXPECT_EQ(i, msgs.size());
	}


	std::vector<std::string> make_test_msgs() {
		return { "foo", "bar", "", std::string(1024, 'c') };
	}

	void test_produce_and_consume(const std::string& t, partition_id_t p, compression_codec_t codec) {
		offset_t old_end = client->get_log_end_offset(t, p).get();

		std::vector<std::string> msgs = make_test_msgs();

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

	test_produce_and_consume("test-topic-1", 1, compression_codec_t::NONE);
}

TEST_F(kafka_client_test_t, simple_compression) {
	make_client();

	test_produce_and_consume("test-topic-1", 2, compression_codec_t::SNAPPY);
}

TEST_F(kafka_client_test_t, compression_not_aligned_fetch) {
	make_client();

	std::vector<std::string> msgs = make_test_msgs();
	client->produce("test-topic-1", 3, make_message_set(msgs, compression_codec_t::SNAPPY)).get();

	std::this_thread::sleep_for(duration_t(0.05));

	offset_t end = client->get_log_end_offset("test-topic-1", 3).get();
	message_set_t msgset = client->fetch("test-topic-1", 3, end - 2).get();

	expect_equal(msgset, make_test_msgs());
}

struct kafka_stress_test_t : public ::testing::Test {
	std::vector<scheduler_ptr_t> schedulers;
	int next_scheduler = 0;
	kafka_client_ptr_t client;

	const std::string topic = "test-topic-2";
	std::vector<offset_t> start_offset;

	const int N_MESSAGES = 1024;
	const int N_PARTITIONS = 1024;
	const int N_PRODUCES = 128;
	const duration_t sleep_time = duration_t(1);

	void SetUp() {
		options_t options;
		options.lib.link_timeout = duration_t(10.0);

		for(int i = 0; i < 4; ++i) {
			schedulers.push_back(make_scheduler());
		}

		broker_list_t brokers = {
			{ "localhost", 9031 }, { "localhost", 9032 }, { "localhost", 9033 }
		};

		client = make_kafka_client(get_next_scheduler(), brokers, options);
	}

	void TearDown() {
		client->shutdown();
	}

	void producer(partition_id_t partition) {
		int msg_idx = 0;
		for(int i = 0; i < N_PRODUCES; ++i) {
			message_set_builder_t builder(4 * 1024 * 1024, compression_codec_t::SNAPPY);
			for(int j = 0; j < N_MESSAGES; ++j) {
				std::string msg = "msg" + std::to_string(++msg_idx);
				builder.append(msg.data(), msg.size());
			}
			client->produce(topic, partition, builder.build()).get();

			duration_t s = sleep_time;
			rt_sleep(&s);
		}
	}

	void consumer(partition_id_t partition) {
		int msg_idx = 0;
		offset_t offset = start_offset[partition];
		while(msg_idx != N_PRODUCES * N_MESSAGES) {
			message_set_t msgset = client->fetch(topic, partition, offset).get();
			auto iter = msgset.iter();
			while(!iter.is_end()) {
				message_t m = iter.next();
				EXPECT_EQ("msg" + std::to_string(++msg_idx), std::string(m.value, m.value_size));
				++offset;
			}

			duration_t s = sleep_time;
			rt_sleep(&s);
		}
	}

	scheduler_ptr_t get_next_scheduler() {
		return schedulers[next_scheduler++ % schedulers.size()];
	}
};

TEST_F(kafka_stress_test_t, stress_test) {
	for(partition_id_t p = 0; p < N_PARTITIONS; ++p) {
		start_offset.push_back(client->get_log_end_offset(topic, p).get());
	}

	std::vector<fiber_t> fibers;
	for(partition_id_t p = 0; p < N_PARTITIONS; ++p) {
		fibers.push_back(get_next_scheduler()->start(&kafka_stress_test_t::producer, this, p));
		fibers.push_back(get_next_scheduler()->start(&kafka_stress_test_t::consumer, this, p));
	}

	for(auto& f : fibers) f.join();
}
