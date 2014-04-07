#pragma once

namespace raptor {

class executor_t {
public:
	virtual ~executor_t() {}

	virtual void run(std::function<void()> task) = 0;
};

// std::function<void()> optionally bound to executor
class closure_t {
public:
	closure_t(executor_t* executor, std::function<void()> task) :
		task_(std::move(task)),
		executor_(executor) {}

	void run() const {
		if(executor_) {
			executor_->run(task_);
		} else {
			task_();
		}
	}

private:
	std::function<void()> task_;
	executor_t* executor_;
};

} // namespace raptor
