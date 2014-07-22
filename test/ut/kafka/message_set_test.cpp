#include <gtest/gtest.h>
#include <cstring>

#include <raptor/kafka/message_set.h>

using namespace raptor::kafka;

TEST(message_set_builder_t, DISABLED_buffer_overflow) {
	message_set_builder_t builder(1024);

	const char* msg = "aaaaaa";

	for(int i = 0; i < 1024 / 32; ++i) {
		ASSERT_TRUE(builder.append(msg, std::strlen(msg)));
	}

	ASSERT_FALSE(builder.append(msg, std::strlen(msg)));
}

TEST(message_set_builder_t, DISABLED_validate) {
	message_set_builder_t builder(1024);

	ASSERT_TRUE(builder.append("1234", 4));

	message_t msg;
	msg.key = "abc";
	msg.key_size = 3;
	msg.value = "dsfasdfa";
	msg.value_size = std::strlen("dsfasdfa");
	ASSERT_TRUE(builder.append(msg));

	auto set = builder.build();
//	set.validate();
}

TEST(message_set_builder_t, DISABLED_compression) {
	message_set_builder_t builder(1024, compression_codec_t::SNAPPY);

	ASSERT_TRUE(builder.append("1234", 4));
	ASSERT_TRUE(builder.append("5678", 4));

	message_set_t msgset = builder.build();
//	msgset.validate(false);

	auto iter = msgset.iter();
	ASSERT_FALSE(iter.is_end());

	message_t msg = iter.next();
	ASSERT_EQ((int8_t)compression_codec_t::SNAPPY, msg.flags);
	ASSERT_TRUE(iter.is_end());

//	msgset.validate(true);
	iter = msgset.iter();
	ASSERT_FALSE(iter.is_end());
	msg = iter.next();
	ASSERT_EQ(std::string("1234"), std::string(msg.value, msg.value_size));
	ASSERT_FALSE(iter.is_end());
	msg = iter.next();
	ASSERT_EQ(std::string("5678"), std::string(msg.value, msg.value_size));
	ASSERT_TRUE(iter.is_end());
}
