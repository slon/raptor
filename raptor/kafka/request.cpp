#include <raptor/kafka/request.h>

namespace raptor { namespace io_kafka {

int32_t wire_string_size(const std::string& str) {
	return 2 + str.size();
}

void request_t::write(wire_writer_t* writer) const {
	writer->int32(header_size() + body_size());
	write_header(writer);
	write_body(writer);
}

void request_t::set_correlation_id(int32_t cid) {
	correlation_id_ = cid;
}

void request_t::set_client_id(const std::string& client_id) {
	client_id_ = client_id;
}

void request_t::write_header(wire_writer_t* writer) const {
	writer->int16(api_key_);
	writer->int16(api_version_);
	writer->int32(correlation_id_);
	writer->string(client_id_);
}

int32_t request_t::header_size() const {
	return 2 + 2 + 4 + wire_string_size(client_id_);
}

void metadata_request_t::write_body(wire_writer_t* writer) const {
	writer->start_array(topics_.size());
	for(auto& topic : topics_) {
		writer->string(topic);
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

void fetch_request_t::write_body(wire_writer_t* writer) const {
	writer->int32(-1); // replica_id is -1 on the client
	writer->int32(max_wait_time);
	writer->int32(min_bytes);

	writer->start_array(1);
	writer->string(topic);

	writer->start_array(1);
	writer->int32(partition);
	writer->int64(offset);
	writer->int32(max_bytes);
}

int32_t produce_request_t::body_size() const {
	int32_t size = 0;
	// ack, timeout, array
	size += 2 + 4 + 4;
	// topic_name, array, partition
	size += wire_string_size(topic) + 4 + 4;

	return size + message_set.wire_size();
}

void produce_request_t::write_body(wire_writer_t* writer) const {
	writer->int16(required_acks);
	writer->int32(timeout);

	writer->start_array(1);
	writer->string(topic);

	writer->start_array(1);
	writer->int32(partition);

	message_set.write(writer);
}

int32_t offset_request_t::body_size() const {
	// replica_id + topic_array + topic + partition_array
	return 4 + 4 + wire_string_size(topic) + 4 +
		   4 + 8 + 4; // partition_id + time + max_number_of_offsets
}

void offset_request_t::write_body(wire_writer_t* writer) const {
	writer->int32(-1); // replica_id

	writer->start_array(1); // topics_array
	writer->string(topic); // topic

	writer->start_array(1); // partition_array
	writer->int32(partition_id); // partition_id
	writer->int64(time); // time
	writer->int32(max_number_of_offsets); // max number of offsets
}

}} // namespace raptor::io_kafka
