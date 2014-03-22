#pragma once

#include <phantom/io_kafka/future.h>
#include <phantom/io_kafka/request.h>
#include <phantom/io_kafka/response.h>

namespace phantom { namespace io_kafka {

class link_t {
public:
	virtual future_t<void> send(request_ptr_t request, response_ptr_t response) = 0;
	virtual bool is_closed() = 0;
	virtual void close() = 0;

	virtual ~link_t() {}
};

typedef std::shared_ptr<link_t> link_ptr_t;

}} // namespace phantom::io_kafka
