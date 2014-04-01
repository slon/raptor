#include <thread>

#include <gmock/gmock.h>

#include <raptor/daemon/daemon.h>

 #include <unistd.h>
 #include <signal.h>
 #include <sys/types.h>

using namespace raptor;
using namespace testing;

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
