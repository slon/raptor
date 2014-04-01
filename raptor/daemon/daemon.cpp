#include <raptor/daemon/daemon.h>

#include <glog/logging.h>

#include <signal.h>

namespace raptor {

void setup_raptor() {
	signal(SIGPIPE, SIG_IGN);
}

void shutdown_raptor() {

}

void wait_shutdown() {}

} // namespace raptor
