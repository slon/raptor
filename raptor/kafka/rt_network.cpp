#include <raptor/kafka/rt_network.h>

#include <glog/logging.h>

#include <raptor/core/syscall.h>
#include <raptor/kafka/rt_link.h>
#include <raptor/kafka/request.h>
#include <raptor/kafka/response.h>

namespace raptor { namespace kafka {

rt_network_t::rt_network_t(
		scheduler_t* scheduler,	std::unique_ptr<link_cache_t> link_cache,
		const options_t& options, const broker_list_t& bootstrap_brokers
) : scheduler_(scheduler), link_cache_(std::move(link_cache)), options_(options), bootstrap_brokers_(bootstrap_brokers) {
	refresh_metadata();
}

void rt_network_t::shutdown() {
	std::unique_lock<spinlock_t> guard(metadata_lock_);
	metadata_refresher_.join();
}

void rt_network_t::refresh_metadata() {
	std::unique_lock<spinlock_t> guard(metadata_lock_);

	// if another refresh is in flight, do nothing
	if(metadata_.is_valid() && !metadata_.is_ready()) return;

	promise_t<metadata_t> metadata_promise;
	metadata_ = metadata_promise.get_future();

	metadata_refresher_ = scheduler_->start([metadata_promise, this] () mutable {
		try {
			duration_t backoff = options_.lib.metadata_refresh_backoff;
			rt_sleep(&backoff);

			const auto& addr = get_next_bootstrap_broker();

			metadata_request_ptr_t request = std::make_shared<metadata_request_t>();
			metadata_response_ptr_t response = std::make_shared<metadata_response_t>();

			auto link = link_cache_->connect(addr).get();
			link->send(request, response).get();

			metadata_promise.set_value(metadata_t(*response));
		} catch(std::exception& ) {
			metadata_promise.set_exception(std::current_exception());
		}
	});
}

future_t<metadata_t> rt_network_t::get_metadata() {
	std::unique_lock<spinlock_t> guard(metadata_lock_);
	return metadata_;
}

const broker_addr_t& rt_network_t::get_next_bootstrap_broker() {
	return bootstrap_brokers_[(next_bootstrap_broker_++) % bootstrap_brokers_.size()];
}

future_t<link_ptr_t> rt_network_t::get_link(const std::string& topic, partition_id_t partition) {
	return get_metadata().bind([this, topic, partition] (future_t<metadata_t> metadata) {
		auto host_port = metadata.get().get_partition_leader_addr(topic, partition);

		return link_cache_->connect(host_port);
	});
}

}} // namespace raptor::kafka
