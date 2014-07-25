#include <raptor/kafka/message_set.h>

#include <cassert>
#include <iostream>

#include <boost/crc.hpp>
#include <snappy.h>
#include <glog/logging.h>

#include <raptor/kafka/exception.h>

namespace raptor { namespace kafka {

static const int32_t MAX_MESSAGE_SET_SIZE = 64 * 1024 * 1024; // 64Mb
static const int32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024; // 16Mb

static const char XERIAL_SNAPPY_MAGIC[8] = { -126, 'S', 'N', 'A', 'P', 'P', 'Y', 0 };

std::unique_ptr<io_buff_t> snappy_decompress(char const* data, size_t data_size) {
	size_t uncompressed;
	if(!snappy::GetUncompressedLength(data, data_size, &uncompressed)) {
		throw std::runtime_error("snappy::GetUncompressedLength() failed");
	}

	auto buf = io_buff_t::create(uncompressed);
	buf->append(uncompressed);

	if(!snappy::RawUncompress(data, data_size, (char*)buf->data())) {
		throw std::runtime_error("snappy::RawUncompress() failed");
	}

	return std::move(buf);
}

std::unique_ptr<io_buff_t> snappy_compress(char const* data, size_t data_size) {
	auto buf = io_buff_t::create(snappy::MaxCompressedLength(data_size));

	size_t compressed_length;
	snappy::RawCompress(data, data_size, (char*)buf->data(), &compressed_length);

	buf->append(compressed_length);
	return std::move(buf);
}

std::unique_ptr<io_buff_t> xerial_snappy_decompress(io_buff_t const* buf) {
	std::unique_ptr<io_buff_t> result;

	char magic[8];
	cursor_t cursor(buf);
	cursor.pull(magic, sizeof(magic));

	// plain old snappy compression
	if(memcmp(magic, XERIAL_SNAPPY_MAGIC, sizeof(magic))) {
		return snappy_decompress((char*)buf->data(), buf->length());
	} else {
		int32_t version = cursor.read_be<int32_t>();
		int32_t compat = cursor.read_be<int32_t>();

		if(version != 1 || compat != 1)
			throw std::runtime_error("unsupported xerial blocking version");

		while(cursor.peek().second != 0) {
			int32_t block_size = cursor.read_be<int32_t>();
			char* block_start = (char*)cursor.data();
			cursor.skip(block_size);

			auto block = snappy_decompress(block_start, block_size);

			if(result) {
				result->prepend_chain(std::move(block));
			} else {
				result = std::move(block);
			}
		}
	}

	if(result) {
		return std::move(result);
	} else {
		return io_buff_t::create(0);
	}
}

std::unique_ptr<io_buff_t> xerial_snappy_compress(io_buff_t const* buf) {
	std::unique_ptr<io_buff_t> result = io_buff_t::create(16 + 8);
	appender_t appender(result.get(), 8);

	appender.push((uint8_t const*)XERIAL_SNAPPY_MAGIC, sizeof(XERIAL_SNAPPY_MAGIC));
	appender.write_be<int32_t>(1); // version
	appender.write_be<int32_t>(1); // compat

	const size_t BLOCK_SIZE = 32 * 1024;

	cursor_t cursor(buf);
	while(cursor.length()) {
		size_t compressed = std::min(cursor.length(), BLOCK_SIZE);
		auto block = snappy_compress((char const*)cursor.data(), compressed);
		cursor.skip(compressed);

		appender.write_be<int32_t>(block->length());
		appender.insert(std::move(block));
	}

	return std::move(result);
}

size_t message_size(message_t msg) {
	size_t msg_size = 4 + // crc
					  1 + // version
					  1 + // flags
					  4 + msg.key_size + // key
					  4 + msg.value_size; //value

	return msg_size;
}

size_t full_message_size(message_t msg) {
	size_t msg_size = message_size(msg);

	size_t full_size = 8 + // offset
					   4 + // size
					   msg_size;

	return full_size;
}

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
	return 4 + data_->compute_chain_data_length();
}

void message_set_t::write(wire_appender_t* appender) const {
	size_t length = data_->compute_chain_data_length();
	appender->int32(length);

	appender->append(data_->clone());
}

void message_set_t::read(wire_cursor_t* cursor) {
	int32_t size = check_range(cursor->int32(), MAX_MESSAGE_SET_SIZE, "messageset.size");

	if(size > 0) {
		data_ = validate(cursor->raw(size));
	}
}

void append_block(std::unique_ptr<io_buff_t>& chain, wire_cursor_t start, wire_cursor_t end) {
	cursor_t s = start.cursor(), e = end.cursor();
	if(!(s == e)) {
		std::unique_ptr<io_buff_t> block;
		s.clone(block, e - s);
		if(chain) {
			chain->prepend_chain(std::move(block));
		} else {
			chain = std::move(block);
		}
	}
}

void message_set_t::validate(bool decompress) {
	data_ = validate(std::move(data_), decompress, true);
	data_->coalesce();
}

std::unique_ptr<io_buff_t> message_set_t::validate(
		std::unique_ptr<io_buff_t>&& raw,
		bool decompress,
		bool allow_compression) {
	std::unique_ptr<io_buff_t> validated;

	wire_cursor_t block_start(raw.get()), block_end = block_start;

	while(true) {
		wire_cursor_t cursor = block_end;

		if(cursor.peek() < 12) {
			break;
		}

		cursor.int64(); // ignore offset
		int32_t msg_size = check_range(cursor.int32(), MAX_MESSAGE_SIZE, "msg.size");

		if(cursor.peek() < (size_t)msg_size) {
			break;
		}

		int32_t crc = cursor.int32();

		boost::crc_32_type compute_crc;
		compute_crc.process_bytes(cursor.data(), msg_size - 4);

		if(static_cast<uint32_t>(crc) != compute_crc()) {
			throw exception_t(std::string("msg crc don't match:") +
							  " actual=" + std::to_string(compute_crc()) +
							  ", expected=" + std::to_string(static_cast<uint32_t>(crc)));
		}

		if(0 != cursor.int8()) {
			throw exception_t("unsupported msg version");
		}

		int8_t compression = cursor.int8();
		if(compression != 0 && compression != (int8_t)compression_codec_t::SNAPPY) {
			throw exception_t("unsupported compression");
		} else if(compression != 0 && !allow_compression) {
			throw exception_t("recursive compression not allowed");
		} else if(decompress && compression == (int8_t)compression_codec_t::SNAPPY) {
			append_block(validated, block_start, block_end);

			cursor.skip_bytes(); // skip key

			int32_t value_size = cursor.int32();
			if(value_size == -1) throw exception_t("message set value is null");
			auto value = cursor.raw(value_size);

			auto uncompressed = xerial_snappy_decompress(value.get());
			uncompressed->coalesce();
			uncompressed = validate(std::move(uncompressed), false, false);

			if(validated) {
				validated->prepend_chain(std::move(uncompressed));
			} else {
				validated = std::move(uncompressed);
			}

			block_start = block_end = cursor;
		} else {
			cursor.skip_bytes(); // skip key
			cursor.skip_bytes(); // skip value
			block_end = cursor;
		}
	}

	append_block(validated, block_start, block_end);
	if(validated) {
		return std::move(validated);
	} else {
		raw->trim_end(raw->length());
		return std::move(raw);
	}
}

bool message_set_t::iter_t::is_end() {
	return cursor_.peek() == 0;
}

message_t message_set_t::iter_t::next() {
	assert(!is_end());

	message_t msg;

	msg.offset = cursor_.int64();
	cursor_.int32(); // skip size
	cursor_.int32(); // skip crc
	cursor_.int8(); // skip version
	msg.flags = cursor_.int8(); // flags

	int32_t key_size = cursor_.int32();
	if(key_size >= 0) {
		msg.key_size = key_size;
		msg.key = cursor_.data();
		cursor_.skip(key_size);
	} else {
		msg.key = NULL;
		msg.key_size = 0;
	}

	msg.value_size = cursor_.int32();
	msg.value = cursor_.data();
	cursor_.skip(msg.value_size);

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

	size_t msg_size = message_size(msg);
	size_t full_size = full_message_size(msg);
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
	data_->append(writer_.pos() - data_->length());

	if(compression_ == compression_codec_t::NONE) {
		return message_set_t(data_->clone());
	} else if(compression_ == compression_codec_t::SNAPPY) {
		auto compressed = xerial_snappy_compress(data_.get());
		compressed->coalesce();

		message_t msg;
		msg.value = (char*)compressed->data();
		msg.value_size = compressed->length();
		msg.flags = (int8_t)compression_codec_t::SNAPPY;

		size_t full_size = full_message_size(msg);
		message_set_builder_t builder(full_size);

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
