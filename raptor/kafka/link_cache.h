#pragma once

#include <map>

#include <raptor/core/future.h>
#include <raptor/core/scheduler.h>
#include <raptor/core/mutex.h>

#include <raptor/kafka/link.h>
#include <raptor/kafka/options.h>

namespace raptor { namespace kafka {

class link_cache_t {
public:
	virtual ~link_cache_t() {}

	virtual void shutdown() = 0;

	virtual future_t<link_ptr_t> connect(const broker_addr_t& addr) = 0;
};

class rt_link_cache_t : public link_cache_t {
public:
	rt_link_cache_t(scheduler_t* scheduler, const options_t& options) : scheduler_(scheduler), options_(options) {}

	virtual future_t<link_ptr_t> connect(const broker_addr_t& addr);

	virtual void shutdown();

private:
	scheduler_t* scheduler_;
	const options_t options_;

	mutex_t mutex_;
	std::map<broker_addr_t, future_t<link_ptr_t>> active_links_;
};

}} // namespace raptor::kafka
