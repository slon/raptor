#include <raptor/kafka/request.h>

#include <gtest/gtest.h>

#include "common.h"

using namespace raptor::kafka;

void check_serialization(const std::string& expected_hex,
						 const request_t& request) {
	char buffer[16];
	mock_writer_t writer(buffer, 16);

	auto buf = request.serialize();
	buf->coalesce();

	std::string request_str((char*)buf->data(), buf->length());

	ASSERT_EQ(remove_spaces(expected_hex), hexify(request_str));
}

TEST(request_write, metadata) {
	check_serialization(
		"0000001A 0003 0000 00000000 000C" + hexify(DEFAULT_CLIENT_ID) + "00000000",
		metadata_request_t{}
	);

	check_serialization(
		"00000029 0003 0000 00000000 000C" + hexify(DEFAULT_CLIENT_ID) + "00000003" +
		"0000" + "0003" + hexify("foo") + "0006" + hexify("barbar"),
		metadata_request_t{{"", "foo", "barbar"}}
	);
}

TEST(request_write, fetch) {
	// check_serialization(
	//	   "00000023 0001 0000 00000000 0009" + hexify(DEFAULT_CLIENT_ID) +
	//	   "FFFFFFFF 00000080 00010000 00000000",
	//	   fetch_request_t(128, 64 * 1024, std::vector<fetch_topic_t>{})
	// );

	// fetch_topic_t topic(
	//	   "mytopic",
	//	   std::vector<fetch_partition_t>{
	//		   fetch_partition_t(0, 0, 64 * 1024),
	//		   fetch_partition_t(5, 67, 64 * 1024)
	//	   }
	// );

	// ASSERT_EQ(2 + 7 + 4 + 32, topic.size());

	// char buffer[16];
	// mock_writer_t writer(buffer, 16);
	// topic.write(&writer);
	// writer.flush_all();

	// ASSERT_EQ(remove_spaces("0007" + hexify("mytopic") + "00000002" +
	//			 "00000000 0000000000000000 00010000" +
	//			 "00000005 0000000000000043 00010000"),
	//			 hexify(writer.data));
}

TEST(request_write, offset) {
	check_serialization(
		"0000003B 0002 0000 00000000 000C" + hexify(DEFAULT_CLIENT_ID) +
		"FFFFFFFF 00000001 0007" + hexify("mytopic") +
		"00000001 00000000 FFFFFFFFFFFFFFFF 00000001",
		offset_request_t("mytopic", 0, -1, 1)
	);
}
