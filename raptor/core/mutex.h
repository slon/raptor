#pragma once

#include <mutex>

#include <raptor/core/spinlock.h>
#include <raptor/core/wait_queue.h>

namespace raptor {

class mutex_t {
public:
	mutex_t() : locked_(false), queue_(&lock_) {}

	void lock() {
		std::lock_guard<spinlock_t> guard(lock_);

		while(locked_) queue_.wait(nullptr);

		locked_ = true;
	}

	void unlock() {
		std::lock_guard<spinlock_t> guard(lock_);

		locked_ = false;

		queue_.notify_one();
	}

private:
	spinlock_t lock_;
	bool locked_;
	wait_queue_t queue_;
};

} // namespace raptor
