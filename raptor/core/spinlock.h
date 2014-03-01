#pragma once

#include <pthread.h>

namespace raptor {

class spinlock_t {
public:
	spinlock_t(const spinlock_t&) = delete;
	spinlock_t(spinlock_t&&) = delete;
	spinlock_t& operator = (const spinlock_t&) = delete;
	spinlock_t& operator = (spinlock_t&&) = delete;

	inline spinlock_t() {
		pthread_spin_init(&spin, 0);
	}

	inline ~spinlock_t() {
		pthread_spin_destroy(&spin);
	}

	inline void lock() {
		pthread_spin_lock(&spin);
	}

	inline void unlock() {
		pthread_spin_unlock(&spin);
	}

private:
	pthread_spinlock_t spin;
};

} // namespace raptor
