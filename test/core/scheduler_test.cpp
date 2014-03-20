#include <raptor/core/scheduler.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(scheduler_test_t, create_shutdown) {
	scheduler_t s;
	s.shutdown();
}

TEST(scheduler_test_t, start_fiber) {
	bool runned = false;

	scheduler_t s;

	fiber_t f = s.start([&runned] () {
		runned = true;
	});

	f.join();
	s.shutdown();

	EXPECT_EQ(true, runned);
}
