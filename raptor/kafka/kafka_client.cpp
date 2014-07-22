#include <raptor/kafka/kafka_client.h>

namespace raptor { namespace kafka {

kafka_client_ptr_t make_kafka_client(
	scheduler_ptr_t scheduler,
	const broker_list_t& brokers,
	const options_t& options
) {
	auto network = std::make_shared<rt_kafka_network_t>(scheduler, options);
	auto cluster = std::make_shared<rt_kafka_cluster_t>(
		scheduler, network, brokers, options
	);

	return std::make_shared<rt_kafka_client_t>(cluster, options);
}

void rt_kafka_client_t::check_response(char const* name, topic_request_ptr_t request, topic_response_ptr_t response, future_t<void> request_completed) {
	if(request_completed.has_exception()) {
		network_error_meter_.mark();
		request_completed.get();
	}

	if(response->err != kafka_err_t::NO_ERROR) {
		server_error_meter_.mark();
		throw_kafka_err(name, response->err, request->topic, request->partition);
	}
}

rt_kafka_client_t::rt_kafka_client_t(kafka_cluster_ptr_t cluster, const options_t& options) :
		cluster_(cluster), options_(options) {
	rpc_timer_ = pm::get_root().subtree("kafka").timer("rpc");
	network_error_meter_ = pm::get_root().subtree("kafka").meter("network_error");
	server_error_meter_ = pm::get_root().subtree("kafka").meter("server_error");
}

future_t<offset_t> rt_kafka_client_t::get_log_offset(
		const std::string& topic, partition_id_t partition, int64_t time
) {
	offset_request_ptr_t request = std::make_shared<offset_request_t>(topic, partition, time, 1);
	offset_response_ptr_t response = std::make_shared<offset_response_t>();

	return cluster_->send(request, response).then([request, response, this] (future_t<void> future) {
		check_response("offset", request, response, future);

		if(response->offsets.size() != 1)
			throw exception_t("wrong number of offsets returned by server");

		return response->offsets[0];
	});
}

future_t<offset_t> rt_kafka_client_t::get_log_end_offset(const std::string& topic, partition_id_t partition) {
	return get_log_offset(topic, partition, /* time = */-1);
}

future_t<offset_t> rt_kafka_client_t::get_log_start_offset(const std::string& topic, partition_id_t partition) {
	return get_log_offset(topic, partition, /* time = */-2);
}

future_t<message_set_t> rt_kafka_client_t::fetch(
		const std::string& topic, partition_id_t partition, offset_t offset
) {
	fetch_request_ptr_t request = std::make_shared<fetch_request_t>(
		options_.kafka.max_wait_time, options_.kafka.min_bytes,
		topic, partition, offset,
		options_.kafka.max_bytes
	);
	fetch_response_ptr_t response = std::make_shared<fetch_response_t>();

	return cluster_->send(request, response).then([request, response, this] (future_t<void> future) {
		check_response("fetch", request, response, future);

		return response->message_set;
	});
}

future_t<void> rt_kafka_client_t::produce(
		const std::string& topic, partition_id_t partition, message_set_t message_set
) {
	produce_request_ptr_t request = std::make_shared<produce_request_t>(
		options_.kafka.required_acks, options_.kafka.produce_timeout,
		topic, partition, message_set
	);
	produce_response_ptr_t response = (options_.kafka.required_acks != 0) ? std::make_shared<produce_response_t>() : NULL;

	return cluster_->send(request, response).then([request, response, this] (future_t<void> future) {
		check_response("produce", request, response, future);
	});
}

void rt_kafka_client_t::shutdown() {
	cluster_->shutdown();
}

}} // namespace raptor::kafka
