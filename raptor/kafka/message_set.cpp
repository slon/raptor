#include <raptor/kafka/message_set.h>

#include <cassert>
#include <iostream>

#include <boost/crc.hpp>
#include <snappy.h>

#include <raptor/kafka/exception.h>

namespace raptor { namespace kafka {

static const int32_t MAX_MESSAGE_SET_SIZE = 16 * 1024 * 1024; // 16Mb
static const int32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024; // 16Mb

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
	return 4 + data_->length();
}

void message_set_t::write(wire_writer_t* writer) const {
	writer->int32(data_->length());
	writer->raw((char*)data_->data(), data_->length());
}

void message_set_t::read(wire_reader_t* reader) {
	int32_t size = check_range(reader->int32(), 0, MAX_MESSAGE_SET_SIZE, "messageset.size");

	if(size > 0) {
		data_ = reader->raw(size);
		validate();
	}
}


void message_set_t::validate(bool decompress) {
	wire_reader_t reader(data_.get());
	size_t msgset_end = 0;

	while(true) {
		size_t msg_start = reader.pos();
		if(reader.remaining() < (size_t)12) {
			msgset_end = msg_start;
			break;
		}

		int64_t offset = reader.int64();
		int32_t msg_size = check_range(reader.int32(), 0, MAX_MESSAGE_SIZE, "msg.size");

		if(reader.remaining() < (size_t)msg_size) {
			msgset_end = msg_start;
			break;
		}

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

		int8_t compression = reader.int8();
		if(compression != 0 && compression != (int8_t)compression_codec_t::SNAPPY) {
			throw exception_t("unsupported compression");
		}

		reader.skip_bytes(); // skip key
		reader.skip_bytes(); // skip value
	}

	// server may return incomplete part of message, we should just cut it off
	data_->trim_end(data_->length() - msgset_end);
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
	msg.flags = reader_.int8(); // flags

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
	return iter_t(data_.get());
}

message_set_builder_t::message_set_builder_t(size_t max_size,
			compression_codec_t compression)
		: max_size_(max_size), compression_(compression) {
	reset();
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
	writer_.int8(msg.flags); // flags

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
	compute_crc.process_bytes(writer_.ptr() + 4, end_pos - crc_pos - 4);
	writer_.int32(static_cast<int32_t>(compute_crc()));
	writer_.set_pos(end_pos);

	return true;
}

void message_set_builder_t::reset() {
	data_ = io_buff_t::create(max_size_);
	writer_ = blob_writer_t((char*)data_->data(), max_size_);
}

message_set_t message_set_builder_t::build() {
	if(compression_ == compression_codec_t::NONE) {
		data_->append(writer_.pos() - data_->length());
		return message_set_t(data_->clone());
	} else if(compression_ == compression_codec_t::SNAPPY) {
		std::string compressed;
		snappy::Compress((char*)data_->data(), writer_.pos(), &compressed);

		size_t msg_size = 4 + // crc
						  1 + // version
						  1 + // flags
						  4 + // key is null
						  4 + compressed.size(); // value

		size_t full_size = 8 + // offset
						   4 + // size
						   msg_size;

		message_set_builder_t builder(full_size);

		message_t msg;
		msg.value = compressed.data();
		msg.value_size = compressed.size();
		msg.flags = (int8_t)compression_codec_t::SNAPPY;

		bool res = builder.append(msg);
		assert(res);

		return builder.build();
	} else {
		throw exception_t("unsupported compression codec: " + std::to_string((int)compression_));
	}
}

bool message_set_builder_t::empty() const {
	return writer_.pos() == 0;
}

}} // namespace raptor::kafka
