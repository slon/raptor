#pragma once

#include <memory>
#include <string>

#include <raptor/core/future.h>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/link.h>

namespace raptor { namespace kafka {

class network_t {
public:
	virtual void add_broker(const std::string& host, uint16_t port) = 0;

	virtual void refresh_metadata() = 0;

	virtual void shutdown() = 0;

	virtual future_t<link_ptr_t> get_link(const std::string& topic, partition_id_t partition) = 0;

	virtual ~network_t() {}
};

}} // namespace raptor::kafka
