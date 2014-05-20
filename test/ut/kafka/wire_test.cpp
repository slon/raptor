#include <raptor/kafka/wire.h>

#include <gtest/gtest.h>

#include "mock.h"

void check(mock_writer_t* writer, const std::string& hex) {
	writer->flush_all();

	ASSERT_EQ(remove_spaces(hex), hexify(writer->data));

	writer->data.clear();
}

TEST(wire_writer_t, ints) {
	char buffer[1];

	mock_writer_t writer(buffer, 1);

	writer.int8(127);
	writer.int8(-1);

	check(&writer, "7F FF");

	writer.int16(0xAAFF);
	check(&writer, "AAFF");

	writer.int32(0xABCD0123);
	check(&writer, "ABCD0123");

	writer.int64(0x0123456789ABCDEF);
	check(&writer, "0123456789ABCDEF");
}

TEST(wire_reader_t, ints) {
	auto buff = make_buff(unhexify("7FFF"));
	wire_reader_t reader(buff.get());

	ASSERT_EQ(127, reader.int8());
	ASSERT_EQ(-1, reader.int8());

	buff = make_buff(unhexify("AAFF"));
	reader = wire_reader_t(buff.get());
	ASSERT_EQ((int16_t)0xAAFF, reader.int16());

	buff = make_buff(unhexify("ABCD0123"));
	reader = wire_reader_t(buff.get());
	ASSERT_EQ((int32_t)0xABCD0123, reader.int32());

	buff = make_buff(unhexify("0123456789ABCDEF"));
	reader = wire_reader_t(buff.get());
	ASSERT_EQ(0x0123456789ABCDEF, reader.int64());
}
