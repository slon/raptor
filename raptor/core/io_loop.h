#pragma once

#include <memory>

#include <raptor/core/executor.h>
#include <raptor/core/time.h>
#include <raptor/core/closure.h>

namespace raptor {

class event_loop_t : public executor_t {
public:
	struct watcher_t {
		virtual void cancel() = 0;
	};

	typedef std::shared_ptr<watcher_t> watcher_ptr_t;

	watcher_ptr_t make_watcher();

	enum run_flag_t { RUN_ONCE = 1 << 0, RUN_NOWAIT = 1 << 1 };
	void run_loop(run_flag_t flag = 0);
	void wakeup_loop();
	void break_loop();

	enum fd_state_t { READ = 1 << 0, WRITE = 1 << 1 };
	void on_fd_ready(const watcher_ptr_t& watcher, closure_t cb, int fd, int events);

	void on_timeout(const watcher_ptr_t& watcher, closure_t cb, interval_t timeout);
};

} // namespace raptor
