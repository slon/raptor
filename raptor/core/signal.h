#pragma once

#include <raptor/core/spinlock.h>
#include <raptor/core/wait_queue.h>

namespace raptor {

class signal_t {
public:
	signal_t() : ready_(false), queue_(&lock_) {}

	bool wait(duration_t* timeout = nullptr) {
		std::unique_lock<spinlock_t> guard(lock_);
		while(!ready_) {
			if(!queue_.wait(timeout))
				return false;
		}

		return true;
	}

	void signal() {
		std::unique_lock<spinlock_t> guard(lock_);
		ready_ = true;
		queue_.notify_all();
	}

private:
	spinlock_t lock_;
	bool ready_;
	wait_queue_t queue_;
};

} // namespace raptor
