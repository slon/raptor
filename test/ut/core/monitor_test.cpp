#include <raptor/core/monitor.h>

#include <thread>

#include <gmock/gmock.h>

#include <raptor/core/impl.h>


using namespace raptor;
using namespace testing;

struct monitor_test_t : public Test {
	scheduler_impl_t scheduler;
};

TEST_F(monitor_test_t, stress_lock_unlock) {
	monitor_t monitor;

	int i = 0;

	auto stress_loop = [&i, &monitor] () {
		for(size_t j = 0; j < 1000000; ++j) {
			monitor.lock(); ++i; monitor.unlock();
		}
	};

	std::thread t1(stress_loop), t2(stress_loop);
	t1.join(); t2.join();

	EXPECT_EQ(2000000, i);
}

TEST_F(monitor_test_t, fiber_wait) {
	monitor_t monitor;

	int wait_res = -1;
	fiber_impl_t fiber([&wait_res, &monitor] () {
		monitor.lock();
		wait_res = monitor.wait(nullptr);
		monitor.unlock();
	});

	scheduler.activate(&fiber);

	scheduler.run(EVRUN_NOWAIT);

	ASSERT_FALSE(fiber.is_terminated());
	ASSERT_EQ(-1, wait_res);

	monitor.lock();
	monitor.notify_one();
	monitor.unlock();

	scheduler.run(EVRUN_NOWAIT);

	ASSERT_TRUE(fiber.is_terminated());
	ASSERT_TRUE(wait_res);
}

TEST_F(monitor_test_t, fiber_wait_timeout) {
	monitor_t monitor;

	int wait_res = -1;
	duration_t timeout = std::chrono::milliseconds(10);
	fiber_impl_t fiber([&wait_res, &timeout, &monitor] () {
		monitor.lock();
		wait_res = monitor.wait(&timeout);
		monitor.unlock();
	});

	scheduler.activate(&fiber);
	scheduler.run(EVRUN_NOWAIT);

	ASSERT_FALSE(fiber.is_terminated());
	ASSERT_EQ(-1, wait_res);

	scheduler.run(EVRUN_ONCE);
	scheduler.run(EVRUN_ONCE);

	ASSERT_TRUE(fiber.is_terminated());
	ASSERT_FALSE(wait_res);
}
