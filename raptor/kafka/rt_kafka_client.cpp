#include <raptor/kafka/rt_kafka_client.h>

#include <raptor/kafka/rt_link.h>
#include <raptor/kafka/rt_network.h>
#include <raptor/kafka/request.h>
#include <raptor/kafka/response.h>

namespace raptor { namespace kafka {

rt_kafka_client_t::rt_kafka_client_t(scheduler_t* scheduler, const broker_list_t& broker_list, const options_t& options) :
	options(options), network(
		std::make_shared<rt_network_t>(scheduler, std::unique_ptr<link_cache_t>(new rt_link_cache_t(scheduler, options)), options, broker_list)
	) {
	rpc_timer = pm::get_root().subtree("kafka").timer("rpc");
	network_error_meter = pm::get_root().subtree("kafka").meter("network_error");
	server_error_meter = pm::get_root().subtree("kafka").meter("server_error");

	network->refresh_metadata();
}

future_t<offset_t> rt_kafka_client_t::get_log_offset(
		const std::string& topic, partition_id_t partition, int64_t time
) {
	offset_request_ptr_t request = std::make_shared<offset_request_t>(topic, partition, time, 1);
	offset_response_ptr_t response = std::make_shared<offset_response_t>();

	auto that = shared_from_this();
	return send(topic, partition, request, response).then([request, response, this, that] (future_t<void> future) {
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
		options.kafka.max_wait_time, options.kafka.min_bytes,
		topic, partition, offset,
		options.kafka.max_bytes
	);
	fetch_response_ptr_t response = std::make_shared<fetch_response_t>();

	auto that = shared_from_this();
	return send(topic, partition, request, response).then([request, response, this, that] (future_t<void> future) {
		check_response("fetch", request, response, future);

		return response->message_set;
	});
}

future_t<void> rt_kafka_client_t::produce(
		const std::string& topic, partition_id_t partition, message_set_t message_set
) {
	produce_request_ptr_t request = std::make_shared<produce_request_t>(
		options.kafka.required_acks, options.kafka.produce_timeout,
		topic, partition, message_set
	);
	produce_response_ptr_t response = (options.kafka.required_acks != 0) ? std::make_shared<produce_response_t>() : NULL;

	auto that = shared_from_this();
	return send(topic, partition, request, response).then([request, response, this, that] (future_t<void> future) {
		check_response("produce", request, response, future);
	});
}

future_t<void> rt_kafka_client_t::send(const std::string& topic, partition_id_t partition, request_ptr_t request, response_ptr_t response) {
	auto start_time = rpc_timer.start();

	future_t<void> sended = network->get_link(topic, partition).bind([request, response] (future_t<link_ptr_t> link) {
		return link.get()->send(request, response);
	});

	auto that = shared_from_this();
	sended.subscribe([start_time, this, that] (future_t<void>) { rpc_timer.finish(start_time); });

	return sended;
}

void rt_kafka_client_t::shutdown() {
	network->shutdown();
}

}} // namespace raptor::kafka
