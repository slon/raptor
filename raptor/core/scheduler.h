#pragma once

#include <memory>

#include <raptor/core/fiber.h>
#include <raptor/core/time.h>
#include <raptor/core/no_copy_or_move.h>

namespace raptor {

struct scheduler_state_t;

class scheduler_t : public no_copy_or_move_t {
public:
	scheduler_t();
	~scheduler_t();

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

typedef std::shared_ptr<scheduler_t> scheduler_ptr_t;

} // namespace raptor
