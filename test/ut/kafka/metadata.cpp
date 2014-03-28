#include <raptor/kafka/metadata.h>

#include <gtest/gtest.h>

#include <algorithm>

#include <raptor/kafka/exception.h>
#include <raptor/kafka/response.h>

using namespace raptor::kafka;

TEST(metadata_t, empty_throws) {
	metadata_t meta;

	ASSERT_THROW(meta.get_host_addr(0), exception_t);
	ASSERT_THROW(meta.get_partition_leader("my_topic", 0), exception_t);
	ASSERT_THROW(meta.get_next_broker(), exception_t);
}

TEST(metadata_t, add_broker) {
	metadata_t meta;

	meta.add_broker("test1.com", 9999);

	auto host = meta.get_next_broker();
	ASSERT_EQ(host.hostname, "test1.com");
	ASSERT_EQ(host.port, 9999);

	host = meta.get_next_broker();
	ASSERT_EQ(host.hostname, "test1.com");
	ASSERT_EQ(host.port, 9999);

	meta.add_broker("test2.com", 8888);
	auto host1 = meta.get_next_broker(),
		 host2 = meta.get_next_broker();

	if(host1.hostname == "test2.com") {
		std::swap(host1, host2);
	}

	ASSERT_EQ(host1.hostname, "test1.com");
	ASSERT_EQ(host1.port, 9999);
	ASSERT_EQ(host2.hostname, "test2.com");
	ASSERT_EQ(host2.port, 8888);
}

TEST(metadata_t, empty_response) {
	metadata_t meta;

	metadata_response_t response({}, {});
	ASSERT_THROW(meta.get_host_addr(0), exception_t);
	ASSERT_THROW(meta.get_partition_leader("my_topic", 0), exception_t);
}

TEST(metadata_t, good_response) {
	metadata_t meta;

	metadata_response_t response(
		{ { 42, "test2.com", 8888 } },
		{ { kafka_err_t::UNKNOWN, "topic1", {
			  { kafka_err_t::UNKNOWN, 0, -1, { -1, 42 }, { 42 } }
		  } },
		  { kafka_err_t::NO_ERROR, "topic2", {
			  { kafka_err_t::NO_ERROR, 0, 42, { 42, 43 }, { 42 } },
			  { kafka_err_t::NO_ERROR, 2, 42, { 42, 43 }, { 42 } }
		  } },
		}
	);

	meta.update(response);

	ASSERT_THROW(meta.get_host_addr(0), exception_t);
	ASSERT_EQ(-1, meta.get_partition_leader("topic1", 0));

	auto host = meta.get_host_addr(42);
	ASSERT_EQ(host.hostname, "test2.com");
	ASSERT_EQ(host.port, 8888);

	ASSERT_EQ(meta.get_partition_leader("topic2", 0), 42);
	ASSERT_THROW(meta.get_partition_leader("topic2", 1), exception_t);
	ASSERT_EQ(meta.get_partition_leader("topic2", 2), 42);
}
