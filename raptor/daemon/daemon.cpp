#include <raptor/daemon/daemon.h>

#include <glog/logging.h>

#include <signal.h>
#include <glog/logging.h>

namespace raptor {

void setup_raptor() {
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	signal(SIGPIPE, SIG_IGN);
}

void shutdown_raptor() {}

void wait_shutdown() {
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	siginfo_t info;
	sigwaitinfo(&mask, &info);
	LOG(INFO) << "Shutting down after SIGTERM from pid: " << info.si_pid
		<< " uind: " << info.si_uid;
}

}  // namespace raptor
