#include <raptor/core/periodic.h>

#include <glog/logging.h>

#include <raptor/core/syscall.h>

namespace raptor {

periodic_t::periodic_t(scheduler_t* scheduler, duration_t interval, std::function<void()> task) :
	shutdown_(false),
	loop_fiber_(scheduler->start(&periodic_t::loop, this, interval, task)) {}

void periodic_t::shutdown() {
	shutdown_ = true;
	loop_fiber_.join();
}

void periodic_t::loop(duration_t interval, std::function<void()> task) {
	time_point_t task_scheduled_time = std::chrono::system_clock::now();

	while(!shutdown_) {
		if(std::chrono::system_clock::now() >= task_scheduled_time) {
			task_scheduled_time += interval;

			try {
				task();
			} catch(const std::exception& e) {
				LOG(ERROR) << e.what();
			}
		}

		duration_t sleep_time = std::min(duration_t(1.0), interval);
		rt_sleep(&sleep_time);
	}
}

} // namespace raptor
