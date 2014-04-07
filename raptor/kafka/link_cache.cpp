#include <raptor/kafka/link_cache.h>

#include <raptor/core/syscall.h>
#include <raptor/io/fd_guard.h>
#include <raptor/kafka/rt_link.h>

namespace raptor { namespace kafka {

future_t<link_ptr_t> rt_link_cache_t::connect(const broker_addr_t& addr) {
	std::lock_guard<mutex_t> guard(mutex_);

	try {
		auto& link = active_links_[addr];
		if(!link.is_valid() || link.has_exception() || (link.has_value() && link.get()->is_closed())) {
			auto link_ptr = std::make_shared<rt_link_t>(connect_socket(addr.first, addr.second), options_);
			link_ptr->start(scheduler_);

			link = make_ready_future(std::static_pointer_cast<link_t>(link_ptr));
		}

		return link;
	} catch(const std::exception& e) {
		return make_exception_future<link_ptr_t>(std::current_exception());
	}
}

}} // namespace raptor::kafka
