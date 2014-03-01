#pragma once

#include <memory>

#include <raptor/core/fiber.h>
#include <raptor/core/time.h>
#include <raptor/core/closure.h>

namespace raptor {

class scheduler_t : public std::enable_shared_from_this<scheduler_t> {
public:
	typedef std::shared_ptr<scheduler_t> ptr;

	virtual fiber_t::ptr run(closure_t task) = 0;
	virtual void switch_to() = 0;

	virtual void cancel() = 0;
	virtual void join() = 0;

	virtual ~scheduler_t() {}
};

scheduler_t* this_scheduler();

} // namespace raptor
