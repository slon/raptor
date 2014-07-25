#include <raptor/kafka/request.h>

namespace raptor { namespace kafka {

int32_t wire_string_size(const std::string& str) {
	return 2 + str.size();
}

std::unique_ptr<io_buff_t> request_t::serialize() const {
	auto buf = io_buff_t::create(ensure_size());
	wire_appender_t appender(buf.get(), 0);

	appender.int32(header_size() + body_size());
	write_header(&appender);
	write_body(&appender);

	return std::move(buf);
}

void request_t::set_correlation_id(int32_t cid) {
	correlation_id_ = cid;
}

void request_t::set_client_id(const std::string& client_id) {
	client_id_ = client_id;
}

void request_t::write_header(wire_appender_t* appender) const {
	appender->int16(api_key_);
	appender->int16(api_version_);
	appender->int32(correlation_id_);
	appender->string(client_id_);
}

int32_t request_t::header_size() const {
	return 2 + 2 + 4 + wire_string_size(client_id_);
}

void metadata_request_t::write_body(wire_appender_t* appender) const {
	appender->start_array(topics_.size());
	for(auto& topic : topics_) {
		appender->string(topic);
	}
}

int32_t metadata_request_t::body_size() const {
	int32_t size = 4;
	for(auto& topic : topics_) {
		size += wire_string_size(topic);
	}
	return size;
}

int32_t fetch_request_t::body_size() const {
	int32_t header_size = 4 + 4 + 4 + 4;
	int32_t topic_size = wire_string_size(topic) + 4 + 16;

	return header_size + topic_size;
}

void fetch_request_t::write_body(wire_appender_t* appender) const {
	appender->int32(-1); // replica_id is -1 on the client
	appender->int32(max_wait_time);
	appender->int32(min_bytes);

	appender->start_array(1);
	appender->string(topic);

	appender->start_array(1);
	appender->int32(partition);
	appender->int64(offset);
	appender->int32(max_bytes);
}

int32_t produce_request_t::body_size() const {
	int32_t size = 0;
	// ack, timeout, array
	size += 2 + 4 + 4;
	// topic_name, array, partition
	size += wire_string_size(topic) + 4 + 4;

	return size + message_set.wire_size();
}

void produce_request_t::write_body(wire_appender_t* appender) const {
	appender->int16(required_acks);
	appender->int32(timeout);

	appender->start_array(1);
	appender->string(topic);

	appender->start_array(1);
	appender->int32(partition);

	message_set.write(appender);
}

int32_t offset_request_t::body_size() const {
	// replica_id + topic_array + topic + partition_array
	return 4 + 4 + wire_string_size(topic) + 4 +
		   4 + 8 + 4; // partition_id + time + max_number_of_offsets
}

void offset_request_t::write_body(wire_appender_t* appender) const {
	appender->int32(-1); // replica_id

	appender->start_array(1); // topics_array
	appender->string(topic); // topic

	appender->start_array(1); // partition_array
	appender->int32(partition); // partition_id
	appender->int64(time); // time
	appender->int32(max_number_of_offsets); // max number of offsets
}

}} // namespace raptor::kafka
