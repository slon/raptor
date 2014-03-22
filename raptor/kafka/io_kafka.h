#pragma once

#include <map>
#include <memory>

#include <pd/base/config.H>
#include <pd/base/config_list.H>
#include <pd/bq/bq_mutex.H>

#include <phantom/io.H>

#include <phantom/io_kafka/kafka.h>
#include <phantom/io_kafka/metadata.h>
#include <phantom/io_kafka/response.h>
#include <phantom/io_kafka/request.h>
#include <phantom/io_kafka/options.h>
#include <phantom/io_kafka/network.h>
#include <phantom/io_kafka/bq_producer.h>
#include <phantom/io_kafka/bq_consumer.h>

namespace phantom {

class io_kafka_t : public io_kafka::kafka_t, public io_t {
public:
	struct config_t : public io_t::config_t {
		config::list_t<string_t> brokers;
	};

	io_kafka_t(const string_t& name, const config_t& config);

	virtual void init();
	virtual void stat_print() const;
	virtual void fini();
	virtual void run() const;

	virtual io_kafka::bq_producer_t* make_producer(const std::string& topic,
												   partition_id_t partition);

	virtual io_kafka::bq_consumer_t* make_consumer(const std::string& topic,
												   partition_id_t partition,
												   offset_t offset);

	future_t<offset_t> get_log_offset(const std::string& topic, partition_id_t partition, int64_t time);

	virtual future_t<offset_t> get_log_end_offset(
		const std::string& topic, partition_id_t partition
	);

	virtual future_t<offset_t> get_log_start_offset(
		const std::string& topic, partition_id_t partition
	);

	virtual future_t<io_kafka::message_set_t> fetch(
		const std::string& topic, partition_id_t partition, offset_t offset
	);

	virtual future_t<void> produce(
		const std::string& topic, partition_id_t partition, io_kafka::message_set_t message_set
	);

private:
	io_kafka::options_t options;

	std::unique_ptr<io_kafka::network_t> network;

	future_t<void> send(
		const std::string& topic, partition_id_t partition,
		io_kafka::request_ptr_t request, io_kafka::response_ptr_t response
	);
};

} // namespace phantom
