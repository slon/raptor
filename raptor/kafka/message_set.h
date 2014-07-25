#pragma once

#include <vector>
#include <memory>

#include <raptor/io/io_buff.h>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/wire.h>

namespace raptor { namespace kafka {

struct wire_cursor_t;

std::unique_ptr<io_buff_t> xerial_snappy_decompress(io_buff_t const* compressed);

class blob_writer_t : public wire_writer_t {
public:
	blob_writer_t() : wire_writer_t(NULL, 0) {}

	blob_writer_t(char* blob, size_t blob_size)
		: wire_writer_t(blob, blob_size) {}

	size_t pos() const;
	void set_pos(size_t pos);
	char const* ptr() const;
	size_t free_space() const;
	char const* buffer() const;
protected:
	virtual void flush();
};

enum class compression_codec_t : int8_t {
	NONE = 0,
	GZIP = 1,
	SNAPPY = 2
};

struct message_t {
	explicit message_t(const std::string& _value) :
		offset(-1), flags(0), key(NULL), key_size(0),
		value(_value.data()), value_size(_value.size()) {}

	message_t() :
		offset(-1), flags(0), key(NULL), key_size(0),
		value(NULL), value_size(0) {}

	int64_t offset;
	int8_t flags;
	char const* key;
	int32_t key_size;
	char const* value;
	int32_t value_size;
};

class message_set_t {
public:
	message_set_t() {}

	message_set_t(const message_set_t& other) : data_(other.data_->clone()) {}
	message_set_t& operator = (const message_set_t& other) {
		data_ = other.data_->clone();
		return *this;
	}

	message_set_t(message_set_t&&) = default;
	message_set_t& operator = (message_set_t&& other) = default;

	explicit message_set_t(std::unique_ptr<io_buff_t> data) : data_(std::move(data)) {}

	size_t wire_size() const;
	void read(wire_cursor_t* cursor);
	void write(wire_appender_t* appender) const;

	class iter_t {
	public:
		bool is_end();
		message_t next();

	private:
		iter_t(io_buff_t* buff)
			: cursor_(buff) {}

		wire_cursor_t cursor_;

		friend class message_set_t;
	};

	iter_t iter() const;

	void validate(bool decompress);

	static std::unique_ptr<io_buff_t> validate(
		std::unique_ptr<io_buff_t>&& buf,
		bool decompress = true,
		bool allow_compression = true);

private:
	std::unique_ptr<io_buff_t> data_;
};

class message_set_builder_t {
public:
	explicit message_set_builder_t(size_t max_size,
		compression_codec_t codec = compression_codec_t::NONE);

	message_set_t build();

	bool append(char const* value, size_t size);
	bool append(message_t msg);

	void reset();
	bool empty() const;

	message_set_builder_t(const message_set_builder_t&) = delete;
	message_set_builder_t(message_set_builder_t&&) = default;

	message_set_builder_t& operator = (const message_set_builder_t&) = delete;
	message_set_builder_t& operator = (message_set_builder_t&&) = default;

private:
	size_t max_size_;
	compression_codec_t compression_;

	std::unique_ptr<io_buff_t> data_;
	blob_writer_t writer_;
};

}} // namespace raptor::namespace kafka
