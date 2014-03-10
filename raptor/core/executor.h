#pragma once

namespace raptor {

class executor_t {
public:
	virtual run(closure_t task) = 0;
};

} // namespace raptor
