#include <raptor/kafka/options.h>

#include <gtest/gtest.h>

using namespace raptor::kafka;

TEST(parse_broker_list, correct) {
	auto blist2 = parse_broker_list("yandex.net:10");
	auto blist3 = parse_broker_list("yandex1:234,yandex2:123");

	decltype(blist2) ans2 = { { "yandex.net", 10 } };
	decltype(blist3) ans3 = { { "yandex1", 234 }, { "yandex2", 123 } };

	ASSERT_EQ(ans2, blist2);
	ASSERT_EQ(ans3, blist3);
}

TEST(parse_broker_list, incorrect) {
	ASSERT_THROW(parse_broker_list(","), std::runtime_error);
	ASSERT_THROW(parse_broker_list("yandex:"), std::runtime_error);
	ASSERT_THROW(parse_broker_list("yandex"), std::runtime_error);
	ASSERT_THROW(parse_broker_list("yandex:1,"), std::runtime_error);
	ASSERT_THROW(parse_broker_list("yandex:10:10"), std::runtime_error);
}
