#include <raptor/kafka/message_set.h>

#include <cassert>
#include <iostream>

#include <boost/crc.hpp>

#include <raptor/kafka/exception.h>

namespace phantom { namespace io_kafka {

static const int32_t MAX_MESSAGE_SET_SIZE = 16 * 1024 * 1024; // 16Mb
static const int32_t MAX_MESSAGE_SIZE = 64 * 1024; // 64Kb

size_t blob_writer_t::pos() const {
	if(full_) {
		return size_;
	} else {
		return wpos_;
	}
}

void blob_writer_t::set_pos(size_t pos) {
	wpos_ = pos;

	full_ = (wpos_ == size_);
}

char const* blob_writer_t::ptr() const {
	return buffer_ + wpos_;
}

char const* blob_writer_t::buffer() const {
	return buffer_;
}

size_t blob_writer_t::free_space() const {
	return size_ - wpos_;
}

void blob_writer_t::flush() {
	throw exception_t("overflow in blob_writer_t");
}

size_t message_set_t::wire_size() const {
	return 4 + data_.size();
}

void message_set_t::write(wire_writer_t* writer) const {
	writer->int32(data_.size());
	writer->raw(data_.data(), data_.size());
}

void message_set_t::read(wire_reader_t* reader) {
	int32_t size = check_range(reader->int32(), 0, MAX_MESSAGE_SET_SIZE, "messageset.size");

	if(size > 0) {
		data_ = reader->raw(size);

		data_ = data_.slice(0, validate());
	}
}

size_t message_set_t::validate() {
	wire_reader_t reader(data_);

	message_count_ = 0;
	while(true) {
		size_t msg_start = reader.pos();

		if(reader.remaining() < (size_t)12) {
			return msg_start;
		}

		int64_t offset = reader.int64();
		int32_t msg_size = check_range(reader.int32(),
									   0, MAX_MESSAGE_SIZE,
									   "msg.size");

		if(reader.remaining() < (size_t)msg_size) {
			return msg_start;
		}

		max_offset_ = offset;
		++message_count_;

		int32_t crc = reader.int32();

		boost::crc_32_type compute_crc;
		compute_crc.process_bytes(reader.ptr(), msg_size - 4);

		if(static_cast<uint32_t>(crc) != compute_crc()) {
			throw exception_t(std::string("msg crc don't match:") +
							  " actual=" + std::to_string(compute_crc()) +
							  ", expected=" + std::to_string(static_cast<uint32_t>(crc)));
		}

		if(0 != reader.int8()) {
			throw exception_t("unsupported msg version");
		}

		if(0 != reader.int8()) {
			throw exception_t("compression not supported");
		}

		reader.skip_bytes(); // skip key

		int32_t value_size = reader.int32();
		if(value_size < 0) {
			throw exception_t("null value");
		}
		reader.skip(value_size); // skip value
	}
}

bool message_set_t::iter_t::is_end() const {
	return reader_.is_end();
}

message_t message_set_t::iter_t::next() {
	assert(!is_end());

	message_t msg;

	msg.offset = reader_.int64();
	reader_.int32(); // skip size
	reader_.int32(); // skip crc
	reader_.int8(); // skip version
	reader_.int8(); // skip compression

	int32_t key_size = reader_.int32();
	if(key_size >= 0) {
		msg.key_size = key_size;
		msg.key = reader_.ptr();
		reader_.skip(key_size);
	} else {
		msg.key = NULL;
		msg.key_size = 0;
	}

	msg.value_size = reader_.int32();
	msg.value = reader_.ptr();
	reader_.skip(msg.value_size);

	return msg;
}

message_set_t::iter_t message_set_t::iter() const {
	return iter_t(data_);
}

bool message_set_builder_t::append(char const* value, size_t size) {
	message_t msg;

	msg.key = NULL;
	msg.key_size = 0;
	msg.value = value;
	msg.value_size = size;

	return append(msg);
}

bool message_set_builder_t::append(message_t msg) {
	assert(msg.value != NULL);
	assert(msg.key != NULL || msg.key_size == 0);

	size_t msg_size = 4 + // crc
					  1 + // version
					  1 + // flags
					  4 + msg.key_size + // key
					  4 + msg.value_size; //value
	size_t full_size = 8 + // offset
					   4 + // size
					   msg_size;
	if(writer_.free_space() < full_size) {
		return false;
	}

	writer_.int64(0); // offset
	writer_.int32(msg_size); // msg_size

	size_t crc_pos = writer_.pos();

	writer_.int32(0); // crc
	writer_.int8(0); // version
	writer_.int8(0); // flags

	// key
	if(msg.key == NULL) {
		writer_.null_bytes();
	} else {
		writer_.bytes(msg.key, msg.key_size);
	}
	writer_.bytes(msg.value, msg.value_size); // value

	size_t end_pos = writer_.pos();

	// jump writer to crc_field, compute and write crc, jump back
	writer_.set_pos(crc_pos);
	boost::crc_32_type compute_crc;
	compute_crc.process_bytes(writer_.ptr() + 4,
							  end_pos - crc_pos - 4);

	writer_.int32(static_cast<int32_t>(compute_crc()));
	writer_.set_pos(end_pos);

	return true;
}

void message_set_builder_t::reset() {
	buffer_ = make_buffer(size_);
	writer_ = blob_writer_t(buffer_.get(), size_);
}

std::shared_ptr<char> message_set_builder_t::make_buffer(size_t size) {
	return std::shared_ptr<char>(new char[size], [] (char* p) { delete[] p; });
}

message_set_t message_set_builder_t::build() {
	return message_set_t(blob_t(buffer_, writer_.pos()));
}

bool message_set_builder_t::empty() const {
	return writer_.pos() == 0;
}

}} // namespace phantom::io_kafka
