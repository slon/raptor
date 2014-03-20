#pragma once

#include <boost/intrusive/list.hpp>

#include <raptor/core/spinlock.h>
#include <raptor/core/time.h>
#include <raptor/core/no_copy_or_move.h>

namespace bi = boost::intrusive;

namespace raptor {

struct queue_waiter_t : public bi::list_base_hook<> {
	queue_waiter_t() : wakeup_next(false) {}

	bool wakeup_next;
	virtual void wakeup();
};

class wait_queue_t : public no_copy_or_move_t {
public:
	wait_queue_t(spinlock_t* lock) : lock_(lock) {}

	bool wait(duration_t* timeout);
	void notify_one();
	void notify_all();

private:
	spinlock_t* lock_;

	bi::list<queue_waiter_t> waiters_;
};

} // namespace raptor
