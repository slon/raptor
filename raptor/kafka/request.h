#pragma once

#include <string>
#include <vector>
#include <memory>

#include <phantom/io_kafka/defs.h>
#include <phantom/io_kafka/wire.h>
#include <phantom/io_kafka/message_set.h>

namespace phantom { namespace io_kafka {

class request_t {
public:
	void write(wire_writer_t* writer) const;

	void set_correlation_id(int32_t cid);
	void set_client_id(const std::string& client_id);

protected:
	void write_header(wire_writer_t* writer) const;
	int32_t header_size() const;

	request_t(api_key_t api_key) :
		api_key_(static_cast<int16_t>(api_key)),
		api_version_(API_VERSION),
		correlation_id_(0),
		client_id_(DEFAULT_CLIENT_ID) {}

	virtual void write_body(wire_writer_t* writer) const = 0;
	virtual int32_t body_size() const = 0;

	int16_t api_key_, api_version_;
	int32_t correlation_id_;
	std::string client_id_;
};

typedef std::shared_ptr<request_t> request_ptr_t;

class metadata_request_t : public request_t {
public:
	metadata_request_t(std::vector<std::string> topics) :
		request_t(api_key_t::METADATA_REQUEST),
		topics_(topics) {}

	metadata_request_t() :
		request_t(api_key_t::METADATA_REQUEST) {}

private:
	// The topics to produce metadata for. If empty the request will
	// yield metadata for all topics.
	std::vector<std::string> topics_;

protected:
	virtual void write_body(wire_writer_t* writer) const;
	virtual int32_t body_size() const;
};

typedef std::shared_ptr<metadata_request_t> metadata_request_ptr_t;

class fetch_request_t : public request_t {
public:
	fetch_request_t(
			int32_t max_wait_time, int32_t min_bytes,
			const std::string& topic, partition_id_t partition, offset_t offset,
			int32_t max_bytes) :
		request_t(api_key_t::FETCH_REQUEST),
		max_wait_time(max_wait_time),
		min_bytes(min_bytes),
		topic(topic),
		partition(partition),
		offset(offset),
		max_bytes(max_bytes) {}

	virtual int32_t body_size() const;
	virtual void write_body(wire_writer_t* writer) const;

	const int32_t max_wait_time;
	const int32_t min_bytes;

	const std::string topic;
	const partition_id_t partition;
	const offset_t offset;
	const int32_t max_bytes;
};

typedef std::shared_ptr<fetch_request_t> fetch_request_ptr_t;

class produce_request_t : public request_t {
public:
	produce_request_t(
			int16_t required_acks, int32_t timeout,
			const std::string& topic, int32_t partition,
			message_set_t message_set) :
		request_t(api_key_t::PRODUCE_REQUEST),
		required_acks(required_acks),
		timeout(timeout),
		topic(topic),
		partition(partition),
		message_set(message_set) {}

	virtual int32_t body_size() const;
	virtual void write_body(wire_writer_t* writer) const;

	const int16_t required_acks;
	const int32_t timeout;
	const std::string topic;
	const int32_t partition;
	const message_set_t message_set;
};

typedef std::shared_ptr<produce_request_t> produce_request_ptr_t;

class offset_request_t : public request_t {
public:
	offset_request_t(
			const std::string topic, int32_t partition_id,
			int64_t time, int32_t max_number_of_offsets) :
		request_t(api_key_t::OFFSET_REQUEST),
		topic(topic),
		partition_id(partition_id),
		time(time),
		max_number_of_offsets(max_number_of_offsets) {}

	virtual int32_t body_size() const;
	virtual void write_body(wire_writer_t* writer) const;

	const std::string topic;
	const int32_t partition_id;
	const int64_t time;
	const int32_t max_number_of_offsets;
};

typedef std::shared_ptr<offset_request_t> offset_request_ptr_t;

}} // namespace phantom::io_kafka
