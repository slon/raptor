#pragma once

#include <memory>

namespace raptor {

class input_stream_t;
class output_stream_t;

struct task_t {
	promise_t<void> promise;

	virtual void write(output_stream_t* out) = 0;
	virtual void read(input_stream_t* in) = 0;

	virtual ~task_t() {}
};

typedef std::shared_ptr<task_t> task_ptr_t;

class bus_t {
public:
	virtual ~bus_t() {}

	virtual void send(task_ptr_t task) = 0;

	virtual void shutdown() = 0;
};

typedef std::shared_ptr<bus_t> bus_ptr_t;

} // namespace raptor
