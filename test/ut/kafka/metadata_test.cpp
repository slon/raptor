#include <raptor/kafka/metadata.h>

#include <gtest/gtest.h>

#include <algorithm>

#include <raptor/kafka/exception.h>
#include <raptor/kafka/response.h>

using namespace raptor::kafka;

TEST(metadata_t, empty_response) {
	metadata_response_t response({}, {});
	metadata_t meta(response);

	ASSERT_THROW(meta.get_partition_leader_addr("my_topic", 0), exception_t);
}

TEST(metadata_t, good_response) {
	metadata_response_t response(
		{ { 42, "test2.com", 8888 } }, {
			{ kafka_err_t::UNKNOWN, "topic1", {
				{ kafka_err_t::UNKNOWN, 0, -1, { -1, 42 }, { 42 } }
			} },
			{ kafka_err_t::NO_ERROR, "topic2", {
				{ kafka_err_t::NO_ERROR, 0, 42, { 42, 43 }, { 42 } },
				{ kafka_err_t::NO_ERROR, 2, 43, { 42, 43 }, { 43 } }
			} },
		}
	);

	metadata_t meta(response);

	ASSERT_THROW(meta.get_partition_leader_addr("topic0", 0), exception_t);
	ASSERT_THROW(meta.get_partition_leader_addr("topic1", 0), exception_t);
	ASSERT_THROW(meta.get_partition_leader_addr("topic2", 1), exception_t);
	ASSERT_THROW(meta.get_partition_leader_addr("topic2", 2), exception_t);

	broker_addr_t addr = { "test2.com", 8888 };
	ASSERT_EQ(addr, meta.get_partition_leader_addr("topic2", 0));
}
