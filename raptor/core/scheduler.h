#pragma once

#include <memory>

#include <raptor/core/fiber.h>
#include <raptor/core/time.h>

namespace raptor {

struct scheduler_state_t;

class scheduler_t {
public:
	scheduler_t();
	~scheduler_t();
	scheduler_t(scheduler_t&& other);
	scheduler_t& operator = (scheduler_t&& other);

	template<class fn_t, class... args_t>
	fiber_t start(fn_t&& fn, args_t&&... args) {
		std::function<void()> task(std::bind(std::forward<fn_t>(fn), std::forward<args_t>(args)...));
		return start(std::move(task));
	}

	fiber_t start(std::function<void()> closure);

	void switch_to();

	void shutdown();

private:
	std::unique_ptr<scheduler_state_t> state_;
};

} // namespace raptor
