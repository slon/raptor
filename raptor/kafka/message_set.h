#pragma once

#include <vector>
#include <memory>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/wire.h>
#include <raptor/kafka/blob.h>

namespace raptor { namespace io_kafka {

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

struct message_t {
	message_t() : offset(-1),
				  key(NULL),
				  key_size(0),
				  value(NULL),
				  value_size(0) {}

	int64_t offset;
	char const* key;
	int32_t key_size;
	char const* value;
	int32_t value_size;
};

class message_set_t {
public:
	message_set_t() : max_offset_(-1), message_count_(0) {}

	explicit message_set_t(blob_t data) : data_(data), max_offset_(-1), message_count_(0) {}

	size_t wire_size() const;
	void read(wire_reader_t* reader);
	void write(wire_writer_t* writer) const;

	class iter_t {
	public:
		bool is_end() const;
		message_t next();

	private:
		iter_t(blob_t data)
			: reader_(data) {}

		wire_reader_t reader_;

		friend class message_set_t;
	};

	iter_t iter() const;

	int64_t max_offset() const { return max_offset_; }
	size_t message_count() const { return message_count_; }

	size_t validate();

private:
	blob_t data_;
	offset_t max_offset_;
	size_t message_count_;
};

class message_set_builder_t {
public:
	explicit message_set_builder_t(size_t max_size) : size_(max_size) {
		reset();
	}

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
	std::shared_ptr<char> buffer_;
	size_t size_;
	blob_writer_t writer_;

	static std::shared_ptr<char> make_buffer(size_t size);
};

}} // namespace raptor::io_kafka
