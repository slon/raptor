#include <raptor/core/wait_queue.h>

#include <thread>

#include <gmock/gmock.h>

#include <raptor/core/impl.h>

using namespace raptor;
using namespace testing;

struct wait_queue_test_t : public Test {
	wait_queue_test_t() : queue(&lock) {}

	scheduler_impl_t scheduler;

	spinlock_t lock;
	wait_queue_t queue;
};

TEST_F(wait_queue_test_t, fiber_wait) {
	int wait_res = -1;
	closure_t task = [&wait_res, this] () {
		lock.lock();
		wait_res = queue.wait(nullptr);
		lock.unlock();
	};
	fiber_impl_t fiber(&task);

	scheduler.activate(&fiber);

	scheduler.run(EVRUN_NOWAIT);

	ASSERT_FALSE(fiber.is_terminated());
	ASSERT_EQ(-1, wait_res);

	lock.lock();
	queue.notify_one();
	lock.unlock();

	scheduler.run(EVRUN_NOWAIT);

	ASSERT_TRUE(fiber.is_terminated());
	ASSERT_EQ(1, wait_res);
}

TEST_F(wait_queue_test_t, fiber_wait_timeout) {
	int wait_res = -1;
	duration_t timeout = std::chrono::milliseconds(10);
	closure_t task = [&wait_res, &timeout, this] () {
		lock.lock();
		wait_res = queue.wait(&timeout);
		lock.unlock();
	};
	fiber_impl_t fiber(&task);

	scheduler.activate(&fiber);
	scheduler.run(EVRUN_NOWAIT);

	ASSERT_FALSE(fiber.is_terminated());
	ASSERT_EQ(-1, wait_res);

	scheduler.run(EVRUN_ONCE);
	scheduler.run(EVRUN_ONCE);

	ASSERT_TRUE(fiber.is_terminated());
	ASSERT_FALSE(wait_res);
	ASSERT_GE(duration_t(0.0), timeout);
}
