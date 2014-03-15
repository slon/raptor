#include <raptor/core/time.h>

#include <gmock/gmock.h>

using namespace raptor;

TEST(duration_test_t, correct_conversion_to_seconds) {
	ASSERT_EQ(1.0, duration_t(1).count());
	ASSERT_EQ(0.001, duration_t(0.001).count());
}
