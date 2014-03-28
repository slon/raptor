#include <gtest/gtest.h>
#include <cstring>

#include <raptor/kafka/message_set.h>

using namespace raptor::kafka;

TEST(message_set_builder_t, buffer_overflow) {
	message_set_builder_t builder(1024);

	const char* msg = "aaaaaa";

	for(int i = 0; i < 1024 / 32; ++i) {
		ASSERT_TRUE(builder.append(msg, std::strlen(msg)));
	}

	ASSERT_FALSE(builder.append(msg, std::strlen(msg)));
}

TEST(message_set_builder_t, validate) {
	message_set_builder_t builder(1024);

	ASSERT_TRUE(builder.append("1234", 4));

	message_t msg;
	msg.key = "abc";
	msg.key_size = 3;
	msg.value = "dsfasdfa";
	msg.value_size = std::strlen("dsfasdfa");
	ASSERT_TRUE(builder.append(msg));

	auto set = builder.build();
	set.validate();
}
