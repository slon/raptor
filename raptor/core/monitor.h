#pragma once

#include <boost/intrusive/list.hpp>

#include <raptor/core/spinlock.h>
#include <raptor/core/time.h>

namespace bi = boost::intrusive;

namespace raptor {

struct monitor_waiter_t : public bi::list_base_hook<> {
	virtual void wakeup();
};

class monitor_t {
public:
	void lock() { lock_.lock(); }
	void unlock() { lock_.unlock(); }

	bool wait(duration_t* timeout);
	void notify_one();
	void notify_all();

private:
	spinlock_t lock_;

	bi::list<monitor_waiter_t> waiters_;
};

} // namespace raptor
