#pragma once

#include <string>
#include <cstdint>
#include <iostream>

#include <raptor/core/time.h>
#include <raptor/io/io_buff.h>
#include <raptor/io/cursor.h>

#include <raptor/kafka/exception.h>

namespace raptor { namespace kafka {

template<class int_t>
int_t check_range(int_t value,
				  int_t max,
				  char const* name) {
	if(value > max) {
		throw std::out_of_range(
			std::string("Error parsing ") +
			name + ", " + std::to_string(value) +
			" > " + std::to_string(max)
		);
	} else {
		return value;
	}
}

template<class int_t>
void check(int_t expected, int_t actual, char const* name) {
	if(expected != actual) {
		throw std::out_of_range(name);
	}
}

class wire_appender_t {
public:
	wire_appender_t(io_buff_t* buff, size_t grow) : appender_(buff, grow) {}

	void ensure(uint32_t n) { appender_.ensure(n); }

	void int8(int8_t i) { appender_.write(i); }
	void int16(int16_t i) { appender_.write_be(i); }
	void int32(int32_t i) { appender_.write_be(i); }
	void int64(int64_t i) { appender_.write_be(i); }

	void start_array(int32_t array_size) { int32(array_size); }

	void null_string() { int16(-1); }
	void null_bytes() { int32(-1); }

	void string(const std::string& str) {
		int16(str.size());
		appender_.push((uint8_t*)str.c_str(), str.size());
	}

	void bytes(const std::string& bytes) {
		int32(bytes.size());
		appender_.push((uint8_t*)bytes.c_str(), bytes.size());
	}

	void bytes(char const* data, size_t size) {
		int32(size);
		appender_.push((uint8_t const*)data, size);
	}

	void append(std::unique_ptr<io_buff_t> buf) {
		appender_.insert(std::move(buf));
	}

private:
	appender_t appender_;
};

class wire_writer_t {
public:
	wire_writer_t(char* buffer, size_t buffer_size)
		: buffer_(buffer),
		  size_(buffer_size),
		  wpos_(0),
		  rpos_(0),
		  full_(false) {};

	virtual ~wire_writer_t();

	void int8(int8_t i);
	void int16(int16_t i);
	void int32(int32_t i);
	void int64(int64_t i);

	void null_string();
	void null_bytes();

	void string(const std::string& str);
	void bytes(const std::string& bytes);
	void bytes(char const* data, size_t size);
	virtual void raw(char const* data, size_t len);

	void start_array(int32_t size);
	void flush_all();

protected:
	char* buffer_;
	size_t size_;
	size_t wpos_, rpos_;
	bool full_;

	virtual void flush() = 0;

	void ensure_not_full();
	bool empty();

	// should be called only after something is written to buffer
	void check_wpos();
};

class rt_wire_writer_t : public wire_writer_t {
public:
	rt_wire_writer_t(int fd, char* buff, size_t size, duration_t* timeout)
		: wire_writer_t(buff, size), fd_(fd), timeout_(timeout) {}

protected:
	virtual void flush();

	int fd_;
	duration_t* timeout_;
};

class wire_cursor_t {
public:
	wire_cursor_t(const io_buff_t* buff) : cursor_(buff) {}

	int8_t int8() { return cursor_.read<int8_t>(); }
	int16_t int16() { return cursor_.read_be<int16_t>(); }
	int32_t int32() { return cursor_.read_be<int32_t>(); }
	int64_t int64() { return cursor_.read_be<int64_t>(); }
	int32_t array_size() { return cursor_.read_be<int32_t>(); }

	bool string(std::string* str) {
		int16_t size = int16();
		if(size == -1) {
			return false;
		} else if(size < -1) {
			throw std::out_of_range("string size < -1");
		} else {
			str->resize(size);
			cursor_.pull(&(*str)[0], size);

			return true;
		}
	}

	std::unique_ptr<io_buff_t> raw(int32_t size) {
		std::unique_ptr<io_buff_t> buff;
		cursor_.clone(buff, size);
		return buff;
	}

	void skip_bytes() {
		int32_t size = int32();
		if(size == -1) {
			return;
		} else if(size < -1) {
			throw std::out_of_range("bytes size < -1");
		} else {
			skip(size);
		}
	}

	void skip(size_t size) {
		cursor_.skip(size);
	}

	char* data() { return (char*)cursor_.data(); }

	size_t peek() { return cursor_.peek().second; }

	cursor_t cursor() { return cursor_; }

private:
	cursor_t cursor_;
};


class wire_reader_t {
public:
	wire_reader_t(io_buff_t* buff) : buff_(buff), pos_(0) {}

	int8_t int8();
	int16_t int16();
	int32_t int32();
	int64_t int64();
	int32_t array_size();

	bool string(std::string* str);

	void raw(int32_t size, char* buffer);
	std::unique_ptr<io_buff_t> raw(int32_t size);

	bool is_end() const;
	char const* ptr() const;
	void skip_bytes();
	void skip(size_t size);
	size_t remaining() const;
	size_t pos() const;
private:
	io_buff_t* buff_;
	size_t pos_;
};

}} // namespace raptor::kafka
