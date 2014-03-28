#pragma once

#include <string>
#include <map>
#include <set>

#include <raptor/kafka/response.h>
#include <raptor/kafka/options.h>

namespace raptor { namespace kafka {

class metadata_t {
public:
	metadata_t(const metadata_response_t& response);

	const broker_addr_t& get_partition_leader_addr(const std::string& topic, partition_id_t partition) const;

private:
	std::map<host_id_t, broker_addr_t> brokers_;
	std::map<std::string, std::map<partition_id_t, host_id_t>> topics_;
};

}} // namespace raptor::kafka
