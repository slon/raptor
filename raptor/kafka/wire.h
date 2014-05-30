#pragma once

#include <string>
#include <cstdint>

#include <raptor/core/time.h>
#include <raptor/kafka/exception.h>
#include <raptor/kafka/blob.h>

namespace raptor { namespace kafka {

template<class int_t>
int_t check_range(int_t value,
                  int_t min,
                  int_t max,
                  char const* name) {
    if(value < min || value > max) {
        throw exception_t(
            std::string("Error parsing ") +
            name + ", " + std::to_string(value) +
            " not in range [" + std::to_string(min) +
            ", " + std::to_string(max) + "]"
        );
    } else {
        return value;
    }
}

template<class int_t>
void check(int_t expected, int_t actual, char const* name) {
    if(expected != actual) {
        throw exception_t(name);
    }
}

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

class wire_reader_t {
public:
    wire_reader_t(blob_t blob)
        : blob_(blob),
          pos_(0) {}

    int8_t int8();
    int16_t int16();
    int32_t int32();
    int64_t int64();
    int32_t array_size();

    bool string(std::string* str);

    void raw(int32_t size, char* buffer);
    blob_t raw(int32_t size);

    bool is_end() const;
    char const* ptr() const;
    void skip_bytes();
    void skip(size_t size);
    size_t remaining() const;
    size_t pos() const;
    void jump(size_t pos);
private:
    blob_t blob_;
    size_t pos_;
};

}} // namespace raptor::kafka
