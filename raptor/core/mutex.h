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

		lock_with_spinlock();
	}

	void unlock() {
		std::lock_guard<spinlock_t> guard(lock_);

		unlock_with_spinlock();
	}

private:
	spinlock_t lock_;
	bool locked_;
	wait_queue_t queue_;

	friend class condition_variable_t;

	void lock_with_spinlock() {
		while(locked_) queue_.wait(nullptr);

		locked_ = true;
	}

	void unlock_with_spinlock() {
		assert(locked_);

		locked_ = false;

		queue_.notify_one();
	}
};

class condition_variable_t {
public:
	condition_variable_t(mutex_t* mutex) : mutex_(mutex), queue_(&mutex->lock_) {}

	bool wait(duration_t* timeout = nullptr) {
		mutex_->unlock_with_spinlock();

		bool res = queue_.wait(timeout);

		mutex_->lock_with_spinlock();

		return res;
	}

	void notify_one() {
		assert(mutex_->locked_);
		queue_.notify_one();
	}

	void notify_all() {
		assert(mutex_->locked_);
		queue_.notify_all();
	}

private:
	mutex_t* mutex_;
	wait_queue_t queue_;
};

class count_down_t {
public:
	count_down_t() : count_(0), queue_(&lock_) {}

	void inc() {
		std::lock_guard<spinlock_t> guard(lock_);
		++count_;
	}

	void dec() {
		std::lock_guard<spinlock_t> guard(lock_);
		--count_;

		if(count_ == 0) queue_.notify_all();
	}

	void wait_zero() {
		std::lock_guard<spinlock_t> guard(lock_);

		while(count_ != 0) queue_.wait(nullptr);
	}

private:
	spinlock_t lock_;
	int count_;
	wait_queue_t queue_;
};

} // namespace raptor
