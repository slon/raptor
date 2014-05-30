#include <raptor/kafka/metadata.h>

#include <glog/logging.h>

#include <raptor/kafka/exception.h>

namespace raptor { namespace kafka {

metadata_t::metadata_t(const metadata_response_t& response) {
	for(const auto& broker : response.brokers()) {
		brokers_[broker.node_id] = { broker.host, static_cast<uint16_t>(broker.port) };
	}

	for(const auto& topic : response.topics()) {
		if(topic.topic_err != kafka_err_t::NO_ERROR) {
			LOG(ERROR) << "Error in topic " << topic.name << " '" << kafka_err_str(topic.topic_err) << "'";
		}

		for(const auto& partition : topic.partitions) {
			if(partition.partition_err != kafka_err_t::NO_ERROR) {
				LOG(ERROR) << "Error in topic " << topic.name
					<< " partition " << partition.partition_id
					<< " '" << kafka_err_str(partition.partition_err) << "'";
			}

			topics_[topic.name][partition.partition_id] = partition.leader;
		}
	}
}

const broker_addr_t& metadata_t::get_partition_leader_addr(const std::string& topic_name, partition_id_t partition_id) const {
	auto topic = topics_.find(topic_name);
	if(topic == topics_.end()) {
		throw unknown_topic_or_partition_t("no topic", topic_name);
	}

	auto partition = topic->second.find(partition_id);
	if(partition == topic->second.end()) {
		throw unknown_topic_or_partition_t(
			"no partition", topic_name, partition_id
		);
	}

	auto broker = brokers_.find(partition->second);
	if(broker == brokers_.end()) {
		throw exception_t("no broker with host_id " + std::to_string(partition->second));
	}

	return broker->second;
}

}} // namespace raptor::kafka
