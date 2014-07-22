#include <raptor/core/scheduler.h>

#include <thread>

#include <raptor/core/impl.h>

namespace raptor {

class single_threaded_scheduler_t : public scheduler_t {
public:
	single_threaded_scheduler_t() {
		thread_ = std::thread([this] () {
			impl_.run();
		});
	}

	virtual ~single_threaded_scheduler_t() {
		shutdown();
	}

	virtual fiber_t start(std::function<void()> closure) {
		fiber_t fiber(std::move(closure));
		impl_.activate(fiber.get_impl());
		return fiber;
	}

	virtual void switch_to() {
		impl_.switch_to();
	}

	virtual void shutdown() {
		if(thread_.joinable()) {
			impl_.break_loop();
			thread_.join();
		}
	}

private:
	std::thread thread_;
	scheduler_impl_t impl_;
};

scheduler_ptr_t make_scheduler(const std::string& ) {
	return std::make_shared<single_threaded_scheduler_t>();
}

} // namespace raptor
