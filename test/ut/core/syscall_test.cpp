#include <raptor/core/syscall.h>

#include <gtest/gtest.h>

#include <unistd.h>

using namespace raptor;

TEST(syscall_test_t, works_from_native_thread) {
	duration_t d(0.01);
	rt_sleep(&d);

	int pipe_fd[2];
	ASSERT_EQ(0, pipe(pipe_fd));
	rt_ctl_nonblock(pipe_fd[0]);
	rt_ctl_nonblock(pipe_fd[1]);

	char buf[10];
	duration_t d2(0.01);
	ASSERT_EQ(-1, rt_read(pipe_fd[0], buf, 10, &d2));
	ASSERT_EQ(errno, ETIMEDOUT);

	close(pipe_fd[0]);
	close(pipe_fd[1]);
}
