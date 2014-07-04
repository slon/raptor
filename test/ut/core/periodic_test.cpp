#include <raptor/core/periodic.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(periodic_test_t, simple) {
	auto s = make_scheduler();

	periodic_t p(s, std::chrono::milliseconds(50), [] () {});
}

TEST(periodic_test_t, runned) {
	auto s = make_scheduler();

	std::atomic<bool> runned(false);

	periodic_t p(s, std::chrono::milliseconds(50), [&runned] () {
		runned = true;
	});

	usleep(10000);

	ASSERT_TRUE(runned);
}

TEST(periodic_test_t, ok_to_throw) {
	auto s = make_scheduler();

	periodic_t p(s, std::chrono::milliseconds(50), [] () {
		throw std::runtime_error("foo");
	});

	usleep(10000);
}
