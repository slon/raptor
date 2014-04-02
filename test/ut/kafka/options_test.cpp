#include <raptor/kafka/options.h>

#include <gtest/gtest.h>

using namespace raptor::kafka;

TEST(split, test) {
	std::vector<std::string> a1 = {}, a2 = { "foo" }, a3 = { "foo", "bar", "zog" };

	ASSERT_EQ(a1, split("", ','));
	ASSERT_EQ(a2, split("foo", ','));
	ASSERT_EQ(a3, split("foo,bar,zog", ','));
}

TEST(parse_broker_list, correct) {
	auto blist2 = parse_broker_list("yandex.net:10");
	auto blist3 = parse_broker_list("yandex1:234,yandex2:123,yandex3:1");

	decltype(blist2) ans2 = { { "yandex.net", 10 } };
	decltype(blist3) ans3 = { { "yandex1", 234 }, { "yandex2", 123 }, { "yandex3", 1 } };

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
