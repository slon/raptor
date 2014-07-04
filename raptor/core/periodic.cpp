#include <raptor/core/periodic.h>

#include <glog/logging.h>

#include <raptor/core/syscall.h>

namespace raptor {

periodic_t::periodic_t(scheduler_ptr_t scheduler, duration_t interval, std::function<void()> task) :
	loop_fiber_(scheduler->start(&periodic_t::loop, this, interval, task)) {}

void periodic_t::shutdown() {
	shutdown_.signal();
	loop_fiber_.join();
}

void periodic_t::loop(duration_t interval, std::function<void()> task) {
	while(true) {
		try {
			task();
		} catch(const std::exception& e) {
			LOG(ERROR) << e.what();
		}

		duration_t sleep_time = interval;
		if(shutdown_.wait(&sleep_time)) {
			break;
		}
	}
}

} // namespace raptor
