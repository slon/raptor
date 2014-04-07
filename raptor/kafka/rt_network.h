#pragma once

#include <atomic>

#include <raptor/core/mutex.h>
#include <raptor/core/scheduler.h>

#include <raptor/io/fd_guard.h>

#include <raptor/kafka/network.h>
#include <raptor/kafka/metadata.h>
#include <raptor/kafka/options.h>
#include <raptor/kafka/link_cache.h>

namespace raptor { namespace kafka {

class rt_network_t : public network_t {
public:
	rt_network_t(
		scheduler_t* scheduler,
		std::unique_ptr<link_cache_t> link_cache,
		const options_t& options,
		const broker_list_t& bootstrap_brokers
	);

	~rt_network_t() { shutdown(); }
	void shutdown();

	virtual void refresh_metadata();
	virtual future_t<link_ptr_t> get_link(const std::string& topic, partition_id_t partition);

private:
	scheduler_t* scheduler_;
	const std::unique_ptr<link_cache_t> link_cache_;
	const options_t options_;

	spinlock_t metadata_lock_;
	future_t<metadata_t> metadata_;
	future_t<metadata_t> get_metadata();

	std::atomic<size_t> next_bootstrap_broker_;
	const broker_list_t bootstrap_brokers_;
	const broker_addr_t& get_next_bootstrap_broker();
};

}} // namespace raptor::kafka
