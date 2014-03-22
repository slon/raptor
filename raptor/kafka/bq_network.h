#pragma once

#include <pd/bq/bq_cond.H>
#include <pd/bq/bq_mutex.H>

#include <phantom/pd.H>
#include <phantom/scheduler.H>

#include <raptor/kafka/network.h>
#include <raptor/kafka/metadata.h>
#include <raptor/kafka/options.h>

namespace raptor { namespace io_kafka {

class bq_network_t : public network_t {
public:
	bq_network_t(scheduler_t* scheduler, const options_t& options);

	virtual void add_broker(const std::string& host, uint16_t port);

	virtual void refresh_metadata();

	virtual future_t<link_ptr_t> get_link(const std::string& topic, partition_id_t partition);

private:
	scheduler_t* scheduler;

	bq_mutex_t mutex;
	bool is_refreshing;

	options_t options;

	metadata_t metadata;

	std::map<host_id_t, std::shared_ptr<link_t>> active_links;

	void do_refresh_metadata();

	std::shared_ptr<link_t> make_link(const std::string& hostname, uint16_t port);
};

}} // namespace raptor::io_kafka
