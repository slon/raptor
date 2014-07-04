#pragma once

#include <memory>

#include <raptor/core/fiber.h>
#include <raptor/core/time.h>

namespace raptor {

class scheduler_t {
public:
	virtual ~scheduler_t() {}

	template<class fn_t, class... args_t>
	fiber_t start(fn_t&& fn, args_t&&... args) {
		std::function<void()> task(std::bind(std::forward<fn_t>(fn), std::forward<args_t>(args)...));
		return start(std::move(task));
	}

	virtual fiber_t start(std::function<void()> closure) = 0;
	virtual void switch_to() = 0;
	virtual void shutdown() = 0;
};

typedef std::shared_ptr<scheduler_t> scheduler_ptr_t;

scheduler_ptr_t make_scheduler(const std::string& name = "default");

} // namespace raptor
