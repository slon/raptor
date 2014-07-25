#pragma once

#include <string>
#include <vector>
#include <memory>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/wire.h>
#include <raptor/kafka/message_set.h>

namespace raptor { namespace kafka {

class request_t {
public:
	std::unique_ptr<io_buff_t> serialize() const;

	void set_correlation_id(int32_t cid);
	void set_client_id(const std::string& client_id);

protected:
	void write_header(wire_appender_t* appender) const;
	int32_t header_size() const;

	request_t(api_key_t api_key) :
		api_key_(static_cast<int16_t>(api_key)),
		api_version_(API_VERSION),
		correlation_id_(0),
		client_id_(DEFAULT_CLIENT_ID) {}

	virtual void write_body(wire_appender_t* appender) const = 0;
	virtual int32_t body_size() const = 0;
	virtual int32_t ensure_size() const { return body_size(); }

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
	virtual void write_body(wire_appender_t* appender) const;
	virtual int32_t body_size() const;
};

typedef std::shared_ptr<metadata_request_t> metadata_request_ptr_t;

class topic_request_t : public request_t {
public:
	topic_request_t(api_key_t key, const std::string& topic, partition_id_t partition) :
		request_t(key),
		topic(topic),
		partition(partition) {}

	const std::string topic;
	const partition_id_t partition;
};

typedef std::shared_ptr<topic_request_t> topic_request_ptr_t;

class fetch_request_t : public topic_request_t {
public:
	fetch_request_t(
			int32_t max_wait_time, int32_t min_bytes,
			const std::string& topic, partition_id_t partition, offset_t offset,
			int32_t max_bytes) :
		topic_request_t(api_key_t::FETCH_REQUEST, topic, partition),
		max_wait_time(max_wait_time),
		min_bytes(min_bytes),
		offset(offset),
		max_bytes(max_bytes) {}

	virtual int32_t body_size() const;
	virtual void write_body(wire_appender_t* appender) const;

	const int32_t max_wait_time;
	const int32_t min_bytes;

	const offset_t offset;
	const int32_t max_bytes;
};

typedef std::shared_ptr<fetch_request_t> fetch_request_ptr_t;

class produce_request_t : public topic_request_t {
public:
	produce_request_t(
			int16_t required_acks, int32_t timeout,
			const std::string& topic, int32_t partition,
			message_set_t message_set) :
		topic_request_t(api_key_t::PRODUCE_REQUEST, topic, partition),
		required_acks(required_acks),
		timeout(timeout),
		message_set(message_set) {}

	virtual int32_t body_size() const;
	virtual void write_body(wire_appender_t* appender) const;

	const int16_t required_acks;
	const int32_t timeout;
	const message_set_t message_set;
};

typedef std::shared_ptr<produce_request_t> produce_request_ptr_t;

class offset_request_t : public topic_request_t {
public:
	offset_request_t(
			const std::string topic, int32_t partition,
			int64_t time, int32_t max_number_of_offsets) :
		topic_request_t(api_key_t::OFFSET_REQUEST, topic, partition),
		time(time),
		max_number_of_offsets(max_number_of_offsets) {}

	virtual int32_t body_size() const;
	virtual void write_body(wire_appender_t* appender) const;

	const int64_t time;
	const int32_t max_number_of_offsets;
};

typedef std::shared_ptr<offset_request_t> offset_request_ptr_t;

}} // namespace raptor::kafka
