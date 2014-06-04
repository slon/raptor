#pragma once

#include <stdexcept>

#include <raptor/kafka/defs.h>

namespace raptor { namespace kafka {

class exception_t : public std::runtime_error {
public:
	exception_t(const std::string& msg)
		: std::runtime_error(msg) {}
};

class server_exception_t : public exception_t {
public:
	server_exception_t(const std::string& msg,
					   kafka_err_t err,
					   const std::string& topic = "",
					   partition_id_t partition = -1) :
		exception_t("server_exception_t: msg: '" + msg +
			"' err: '" + kafka_err_str(err) +
			"' topic: '" + topic +
			"' partition: " + std::to_string(partition)),
		err_(err),
		topic_(topic),
		partition_(partition) {}

	const std::string& topic() const {
		return topic_;
	}

	partition_id_t partition() const {
		return partition_;
	}

	kafka_err_t err() const {
		return err_;
	}

	~server_exception_t() noexcept {}

private:
	kafka_err_t err_;
	std::string topic_;
	partition_id_t partition_;
};

class unknown_topic_or_partition_t : public server_exception_t {
public:
	unknown_topic_or_partition_t(const std::string& msg,
								 const std::string& topic = "",
								 partition_id_t partition = -1)
		: server_exception_t(
			msg,
			kafka_err_t::UNKNOWN_TOPIC_OR_PARTITION,
			topic,
			partition
		) {}
};

class offset_out_of_range_t : public server_exception_t {
public:
	offset_out_of_range_t(const std::string& msg,
						  const std::string& topic = "",
						  partition_id_t partition = -1)
		: server_exception_t(
			msg,
			kafka_err_t::OFFSET_OUT_OF_RANGE,
			topic,
			partition
		) {}
};

void throw_kafka_err(const std::string& msg,
					 kafka_err_t err,
					 const std::string& topic = "",
					 partition_id_t partition = -1);

}} // namespace raptor::kafka
