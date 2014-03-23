#include <raptor/core/mutex.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(mutex_death_test_t, unlock) {
	mutex_t m;
	ASSERT_DEATH(m.unlock(), ".*mutex.*");
}

TEST(mutex_death_test_t, access_condition_without_lock) {
	mutex_t m;
	condition_variable_t v(&m);
	ASSERT_DEATH(v.wait(), ".*Assertion.*");
	ASSERT_DEATH(v.notify_one(), ".*Assertion.*");
	ASSERT_DEATH(v.notify_all(), ".*Assertion.*");
}
