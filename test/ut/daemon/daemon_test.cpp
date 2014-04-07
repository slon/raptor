#include <thread>

#include <gmock/gmock.h>

#include <raptor/daemon/daemon.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

using namespace raptor;
using namespace testing;

TEST(setup_test, sigterm_test) {
	int pid = getpid();
	std::function<void()> task = [pid]() {
		usleep(50);
		kill(pid, SIGPIPE);
	};

	std::thread t(task);
	setup_raptor();
	t.join();
}

TEST(shutdown_test, sigterm_test) {
	int pid = getpid();
	std::function<void()> task = [pid] () {
		usleep(50);
		kill(pid, SIGTERM);
	};
	std::thread t(task);
	wait_shutdown();
	t.join();
}
