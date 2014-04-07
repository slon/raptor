#include <raptor/core/periodic.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(periodic_test_t, simple) {
	scheduler_t s;
	periodic_t p(&s, std::chrono::milliseconds(50), [] () {});
}

TEST(periodic_test_t, runned) {
	scheduler_t s;

	std::atomic<bool> runned(false);

	periodic_t p(&s, std::chrono::milliseconds(50), [&runned] () {
		runned = true;
	});

	usleep(100);

	ASSERT_TRUE(runned);
}

TEST(periodic_test_t, ok_to_throw) {
	scheduler_t s;
	periodic_t p(&s, std::chrono::milliseconds(50), [] () {
		throw std::runtime_error("foo");
	});

	usleep(100);
}
