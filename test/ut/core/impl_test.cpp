#include <raptor/core/impl.h>

#include <unistd.h>
#include <sys/fcntl.h>

#include <thread>

#include <gmock/gmock.h>

using namespace raptor;
using namespace testing;

TEST(scheduler_impl_t, creates) {
	scheduler_impl_t scheduler;

	scheduler.run_once();
}

TEST(DISABLED_scheduler_impl_t, cancel) {
	scheduler_impl_t scheduler;

	std::thread t([&scheduler] () mutable {
		scheduler.run();
	});

	sleep(1);

	scheduler.cancel();

	t.join();
}

TEST(scheduler_impl_t, run_simple_fiber) {
	scheduler_impl_t scheduler;

	bool runned = false;
	fiber_impl_t fiber([&runned] () mutable {
		runned = true;
	});

	scheduler.wakeup(&fiber);
	scheduler.run_once();

	ASSERT_TRUE(runned);
	ASSERT_EQ(fiber_impl_t::TERMINATED, fiber.state());
}

TEST(scheduler_impl_t, wait_io) {
	int fd[2];
	int res = pipe2(fd, O_NONBLOCK);
	ASSERT_TRUE(res == 0);

	scheduler_impl_t scheduler;

	fiber_impl_t fiber([fd] () {
		SCHEDULER_IMPL->wait_io(fd[0], EV_READ);
	});

	scheduler.wakeup(&fiber);
	scheduler.run_once();

	ASSERT_EQ(fiber_impl_t::WAITING, fiber.state());

	ASSERT_TRUE(1 == write(fd[1], "0", 1));

	scheduler.run_once();

	ASSERT_EQ(fiber_impl_t::TERMINATED, fiber.state());
}

struct running_scheduler_impl_t : public Test {
	virtual void SetUp() {
		t = std::thread([this] () { scheduler.run(); });
	}

	virtual void TearDown() {
		scheduler.cancel();
		t.join();
	}

	std::thread t;
	scheduler_impl_t scheduler;
};
