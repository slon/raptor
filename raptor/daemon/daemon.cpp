#include <raptor/daemon/daemon.h>

#include <signal.h>

namespace raptor {

void setup_raptor() {
	signal(SIGPIPE, SIG_IGN);
}

void wait_shutdown() {}

} // namespace raptor
