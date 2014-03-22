#include <raptor/kafka/io_kafka.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include <pd/base/config.H>
#include <pd/bq/bq_thr.H>
#include <pd/bq/bq_job.H>
#include <pd/bq/bq_util.H>
#include <pd/base/log.H>

#include <phantom/scheduler.H>

#include <raptor/kafka/bq_link.h>
#include <raptor/kafka/bq_network.h>
#include <raptor/kafka/request.h>
#include <raptor/kafka/response.h>

namespace phantom {

using namespace io_kafka;

io_kafka_t::io_kafka_t(const pd::string_t& name,
					   const config_t& config) :
		phantom::io_t(name, config),
		options(default_options()),
		network(new bq_network_t(config.scheduler, options)) {
	for(auto ptr = config.brokers._ptr(); ptr; ++ptr) {
		std::string address(ptr.val().ptr(), ptr.val().size());
		size_t split = address.find(":");
		std::string host = address.substr(0, split);
		uint16_t port = 9092;
		if(split != std::string::npos) {
			port = atoi(address.c_str() + split + 1);
		}

		network->add_broker(host, port);
	}
}

bq_producer_t* io_kafka_t::make_producer(const std::string& topic, partition_id_t partition) {
	return NULL;
}

bq_consumer_t* io_kafka_t::make_consumer(const std::string& topic, partition_id_t partition, offset_t offset) {
	return NULL;
}

future_t<offset_t> io_kafka_t::get_log_offset(
		const std::string& topic, partition_id_t partition, int64_t time
) {
	offset_request_ptr_t request = std::make_shared<offset_request_t>(topic, partition, time, 1);
	offset_response_ptr_t response = std::make_shared<offset_response_t>();

	future_t<void> request_completed = send(topic, partition, request, response);

	std::function<offset_t(future_t<void>)> handler = [request, response, this] (future_t<void> future) {
		if(future.has_exception()) {
			network->refresh_metadata();
			future.get();
		}

		if(response->err != kafka_err_t::NO_ERROR) {
			network->refresh_metadata();
			throw_kafka_err("get_log_end_offset", response->err, request->topic, request->partition_id);
		}

		if(response->offsets.size() != 1)
			throw exception_t("wrong number of offsets returned by server");

		return response->offsets[0];
	};

	return request_completed.then(handler);
}

future_t<offset_t> io_kafka_t::get_log_end_offset(const std::string& topic, partition_id_t partition) {
	return get_log_offset(topic, partition, /* time = */-1);
}

future_t<offset_t> io_kafka_t::get_log_start_offset(const std::string& topic, partition_id_t partition) {
	return get_log_offset(topic, partition, /* time = */-2);
}

future_t<io_kafka::message_set_t> io_kafka_t::fetch(
		const std::string& topic, partition_id_t partition, offset_t offset
) {
	fetch_request_ptr_t request = std::make_shared<fetch_request_t>(
		options.kafka.max_wait_time, options.kafka.min_bytes,
		topic, partition, offset,
		options.kafka.max_bytes
	);
	fetch_response_ptr_t response = std::make_shared<fetch_response_t>();

	future_t<void> request_completed = send(topic, partition, request, response);

	std::function<message_set_t(future_t<void>)> handler = [request, response, this] (future_t<void> future) {
		if(future.has_exception()) {
			network->refresh_metadata();
			future.get();
		}

		if(response->err != kafka_err_t::NO_ERROR) {
			network->refresh_metadata();
			throw_kafka_err("fetch", response->err, request->topic, request->partition);
		}

		return response->message_set;
	};

	return request_completed.then(handler);
}

future_t<void> io_kafka_t::produce(
		const std::string& topic, partition_id_t partition, io_kafka::message_set_t message_set
) {
	produce_request_ptr_t request = std::make_shared<produce_request_t>(
		options.kafka.required_acks, options.kafka.produce_timeout,
		topic, partition, message_set
	);
	produce_response_ptr_t response = (options.kafka.required_acks != 0) ? std::make_shared<produce_response_t>() : NULL;

	future_t<void> request_completed = send(topic, partition, request, response);

	std::function<void(future_t<void>)> handler = [request, response, this] (future_t<void> future) {
		if(future.has_exception()) {
			network->refresh_metadata();
			future.get();
		}

		if(response->err != kafka_err_t::NO_ERROR) {
			network->refresh_metadata();
			throw_kafka_err("produce", response->err, request->topic, request->partition);
		}
	};

	return request_completed.then(handler);
}

void io_kafka_t::init() {}
void io_kafka_t::stat_print() const {}
void io_kafka_t::fini() {}
void io_kafka_t::run() const {
	network->refresh_metadata();
}

future_t<void> io_kafka_t::send(const std::string& topic, partition_id_t partition, request_ptr_t request, response_ptr_t response) {
	std::function<future_t<void>(future_t<link_ptr_t>)> send_handler = [request, response] (future_t<link_ptr_t> link) {
		return link.get()->send(request, response);
	};

	return network->get_link(topic, partition).then(send_handler);
}

namespace config_binding {
config_binding_sname(io_kafka_t);
config_binding_parent(io_kafka_t, io_t);
config_binding_ctor(io_t, io_kafka_t);
config_binding_cast(io_kafka_t, kafka_t);

config_binding_value(io_kafka_t, brokers);
}

} // namespace phantom
