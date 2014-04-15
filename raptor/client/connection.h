#pragma once

namespace raptor {

struct request_t {
	virtual ~request_t() {}
};

struct response_t {
	virtual ~response_t() {}
}

struct protocol_t {
	virtual void write(int fd, const request_t& request) = 0;
	virtual void read(int fd, response_t* response) = 0;
	virtual ~protocol_t() {};
};

struct task_t {
	std::shared_ptr<request_t> request;
	std::shared_ptr<response_t> response;
	future_t<void> completed;
};

} // namespace raptor
