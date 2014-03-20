#include <raptor/core/impl.h>

#include <unistd.h>
#include <sys/fcntl.h>

#include <thread>

#include <gmock/gmock.h>

using namespace raptor;
using namespace testing;

TEST(scheduler_impl_t, creates) {
	scheduler_impl_t scheduler;

	scheduler.run(EVRUN_NOWAIT);
}

TEST(scheduler_impl_t, break_loop) {
	scheduler_impl_t scheduler;

	std::thread t([&scheduler] () mutable {
		scheduler.run();
	});

	usleep(10000);

	scheduler.break_loop();

	t.join();
}

TEST(scheduler_impl_t, run_simple_fiber) {
	scheduler_impl_t scheduler;

	bool runned = false;
	closure_t task = [&runned] () mutable {
		runned = true;
	};
	fiber_impl_t fiber(&task);

	scheduler.activate(&fiber);
	scheduler.run(EVRUN_NOWAIT);

	EXPECT_TRUE(runned);
	EXPECT_TRUE(fiber.is_terminated());
}

TEST(scheduler_impl_t, run_terminate_cb) {
	scheduler_impl_t scheduler;

	bool runned = false;
	closure_t task = [] () {};
	closure_t terminate_cb = [&runned] () { runned = true; };
	fiber_impl_t fiber(&task, &terminate_cb);

	scheduler.activate(&fiber);
	scheduler.run(EVRUN_NOWAIT);

	EXPECT_TRUE(runned);
	EXPECT_TRUE(fiber.is_terminated());
}


TEST(scheduler_impl_t, wait_io) {
	int fd[2];
	int res = pipe2(fd, O_NONBLOCK);
	ASSERT_TRUE(res == 0);

	scheduler_impl_t scheduler;

	int wait_res = -1;
	closure_t task = [fd, &wait_res] () {
		wait_res = SCHEDULER_IMPL->wait_io(fd[0], EV_READ, nullptr);
	};
	fiber_impl_t fiber(&task);

	scheduler.activate(&fiber);
	scheduler.run(EVRUN_NOWAIT);

	EXPECT_FALSE(fiber.is_terminated());

	ASSERT_TRUE(1 == write(fd[1], "0", 1));

	scheduler.run(EVRUN_NOWAIT);

	EXPECT_TRUE(fiber.is_terminated());
	EXPECT_EQ(scheduler_impl_t::READY, wait_res);

	close(fd[0]); close(fd[1]);
}

TEST(scheduler_impl_t, wait_io_timeout) {
	int fd[2];
	ASSERT_EQ(0, pipe2(fd, O_NONBLOCK));

	scheduler_impl_t scheduler;

	int wait_res = -1;
	duration_t duration = std::chrono::milliseconds(10);
	closure_t task = [fd, &wait_res, &duration] () {
		wait_res = SCHEDULER_IMPL->wait_io(fd[0], EV_READ, &duration);
	};
	fiber_impl_t fiber(&task);

	scheduler.activate(&fiber);
	scheduler.run(EVRUN_NOWAIT);

	EXPECT_FALSE(fiber.is_terminated());

	scheduler.run(EVRUN_ONCE);
	scheduler.run(EVRUN_ONCE);

	EXPECT_EQ(scheduler_impl_t::TIMEDOUT, wait_res);
	EXPECT_TRUE(fiber.is_terminated());
	EXPECT_GE(duration_t(0.0), duration);

	close(fd[0]); close(fd[1]);
}

TEST(scheduler_impl_t, wait_timeout) {
	scheduler_impl_t scheduler;

	int wait_res = -1;
	duration_t duration = std::chrono::milliseconds(10);
	closure_t task = [&wait_res, &duration] () {
		wait_res = SCHEDULER_IMPL->wait_timeout(&duration);
	};
	fiber_impl_t fiber(&task);

	scheduler.activate(&fiber);
	scheduler.run(EVRUN_NOWAIT);

	EXPECT_FALSE(fiber.is_terminated());

	scheduler.run(EVRUN_ONCE);
	scheduler.run(EVRUN_ONCE);

	EXPECT_EQ(scheduler_impl_t::READY, wait_res);
	EXPECT_TRUE(fiber.is_terminated());
	EXPECT_GE(duration_t(0.0), duration);
}
