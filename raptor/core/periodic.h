#pragma once

#include <atomic>

#include <raptor/core/scheduler.h>
#include <raptor/core/signal.h>

namespace raptor {

class periodic_t {
public:
	periodic_t(scheduler_t* scheduler, duration_t interval, std::function<void()> task);

	~periodic_t() { shutdown(); }

	void shutdown();

private:
	signal_t shutdown_;

	fiber_t loop_fiber_;


	void loop(duration_t interval, std::function<void()> task);
};

} // namespace raptor
