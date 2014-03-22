#pragma once

#include <exception>
#include <iostream>

#include <raptor/kafka/message_set.h>

namespace raptor { namespace io_kafka {

class hooks_t {
public:
	virtual void on_exception(const std::exception_ptr& err) {
		try {
			std::rethrow_exception(err);
		} catch(const std::exception& e) {
			std::cerr << e.what() << std::endl;
		} catch(...) {}
	}

	// true == try send message_set again
	virtual bool on_producer_fail(message_set_t message_set,
								  const std::exception_ptr& err) {
		return false;
	}

	virtual ~hooks_t() {}
};

}} // namespace raptor::io_kafka
