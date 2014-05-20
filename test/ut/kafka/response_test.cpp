#include <raptor/kafka/response.h>

#include <gtest/gtest.h>

#include "mock.h"

using namespace raptor::kafka;

TEST(metadata_response_t, read) {
	auto buff = make_buff(unhexify(
		 std::string("ABCD1234") +
		"00000002" +
		"00000015" + "0009" + hexify("yandex.ru") + "00000AFA" +
		"00000016" + "000A" + hexify("yandex.net") + "00000FAF" +
		"00000000"
	));

	wire_reader_t reader(buff.get());

	metadata_response_t meta;
	meta.read(&reader);

	ASSERT_EQ(meta.brokers().size(), 2U);
	ASSERT_EQ(meta.brokers()[0].node_id, 21);
	ASSERT_EQ(meta.brokers()[1].node_id, 22);
	ASSERT_EQ(meta.brokers()[0].host, "yandex.ru");
	ASSERT_EQ(meta.brokers()[1].host, "yandex.net");
	ASSERT_EQ(meta.brokers()[0].port, 0xAFA);
	ASSERT_EQ(meta.brokers()[1].port, 0xFAF);

	buff = make_buff(unhexify(
		std::string("0000") + "0003" + hexify("foo") +
		"00000001" + "0000" + "00000010" + "00000015" +
					 "00000002" + "00000016" + "00000018"
					 "00000001" + "00000018"
	));

	reader = wire_reader_t(buff.get());

	topic_metadata_t topic;
	topic.read(&reader);

	ASSERT_EQ(kafka_err_t::NO_ERROR, topic.topic_err);
	ASSERT_EQ("foo", topic.name);
	ASSERT_EQ(1U, topic.partitions.size());
}
