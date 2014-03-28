#include <raptor/kafka/metadata.h>

#include <glog/logging.h>

#include <raptor/kafka/exception.h>

namespace raptor { namespace kafka {

void metadata_t::update(const metadata_response_t& response) {
	brokers_.clear();
	topics_.clear();

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
					<< " '" << kafka_err_str(topic.topic_err) << "'";
			}

			topics_[topic.name][partition.partition_id] = partition.leader;
		}
	}
}

metadata_t::addr_t metadata_t::get_host_addr(int32_t host_id) const {
	auto broker = brokers_.find(host_id);
	if(broker == brokers_.end()) {
		throw exception_t("no broker with host_id " + std::to_string(host_id));
	}

	return broker->second;
}

int32_t metadata_t::get_partition_leader(const std::string& topic_name,
										 int32_t partition_id) const {
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

	return partition->second;
}

metadata_t::addr_t metadata_t::get_next_broker() {
	if(bootstrap_brokers_.empty()) {
		throw exception_t("add at least one broker");
	}

	addr_t host = bootstrap_brokers_[next_bootstrap_broker_];

	++next_bootstrap_broker_;
	next_bootstrap_broker_ %= bootstrap_brokers_.size();

	return host;
}

void metadata_t::add_broker(const std::string& host, uint16_t port) {
	bootstrap_brokers_.push_back({ host, port });
}

}} // namespace raptor::kafka
