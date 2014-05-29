#include <raptor/kafka/wire.h>

#include <endian.h>
#include <unistd.h>
#include <sys/uio.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <iostream>
#include <system_error>

#include <raptor/core/syscall.h>

#include <raptor/kafka/exception.h>

namespace raptor { namespace kafka {

static const size_t MAX_STRING_SIZE = 1024;

wire_writer_t::~wire_writer_t() {}

void wire_writer_t::ensure_not_full() {
	while(full_) {
		flush();
	}
}

bool wire_writer_t::empty() {
	return !full_ && wpos_ == rpos_;
}

void wire_writer_t::flush_all() {
	while(!empty()) {
		flush();
	}
}

void wire_writer_t::check_wpos() {
	if(wpos_ == size_) {
		wpos_ -= size_;
	}

	if(wpos_ == rpos_) {
		full_ = true;
	}
}

void wire_writer_t::int8(int8_t i) {
	raw(reinterpret_cast<char*>(&i), sizeof(i));
}

void wire_writer_t::int16(int16_t i) {
	i = htobe16(i);
	raw(reinterpret_cast<char*>(&i), sizeof(i));
}

void wire_writer_t::int32(int32_t i) {
	i = htobe32(i);
	raw(reinterpret_cast<char*>(&i), sizeof(i));
}

void wire_writer_t::int64(int64_t i) {
	i = htobe64(i);
	raw(reinterpret_cast<char*>(&i), sizeof(i));
}

void wire_writer_t::null_string() {
	int16(-1);
}

void wire_writer_t::null_bytes() {
	int32(-1);
}

void wire_writer_t::string(const std::string& str) {
	int16(str.size());

	raw(str.c_str(), str.size());
}

void wire_writer_t::bytes(const std::string& bytes) {
	int32(bytes.size());

	raw(bytes.c_str(), bytes.size());
}

void wire_writer_t::bytes(char const* bytes, size_t size) {
	int32(size);

	raw(bytes, size);
}

void wire_writer_t::raw(char const* data, size_t len) {
	char const* end = data + len;

	while(data < end) {
		ensure_not_full();

		size_t chunk_size = std::min<size_t>(
			end - data,
			(rpos_ <= wpos_) ? (size_ - wpos_) : (rpos_ - wpos_)
		);

		memcpy(buffer_ + wpos_, data, chunk_size);
		data += chunk_size;
		wpos_ += chunk_size;

		check_wpos();
	}
}

void wire_writer_t::start_array(int32_t array_size) {
	int32(array_size);
}

void rt_wire_writer_t::flush() {
	if(empty()) {
		return;
	}

	iovec iov[2];
	int iovec_count = 0;

	if(rpos_ < wpos_) {
		iov[0].iov_base = buffer_ + rpos_;
		iov[0].iov_len = wpos_ - rpos_;
		iovec_count = 1;
	} else {
		iov[0].iov_base = buffer_ + rpos_;
		iov[0].iov_len = size_ - rpos_;

		iov[1].iov_base = buffer_;
		iov[1].iov_len = wpos_;

		iovec_count = 2;
	}

	ssize_t res = rt_writev(fd_, iov, iovec_count, timeout_);
	if(res < 0) {
		throw std::system_error(errno, std::system_category(), "rt_writev()");
	}

	if(res > 0) {
		full_ = false;
	}

	rpos_ += res;
	if(rpos_ >= size_) {
		rpos_ -= size_;
	}
}

int8_t wire_reader_t::int8() {
	int8_t i;
	raw(sizeof(i), reinterpret_cast<char*>(&i));
	return i;
}

int16_t wire_reader_t::int16() {
	int16_t i;
	raw(sizeof(i), reinterpret_cast<char*>(&i));
	return be16toh(i);
}

int32_t wire_reader_t::int32() {
	int32_t i;
	raw(sizeof(i), reinterpret_cast<char*>(&i));
	return be32toh(i);
}

int64_t wire_reader_t::int64() {
	int64_t i;
	raw(sizeof(i), reinterpret_cast<char*>(&i));
	return be64toh(i);
}

bool wire_reader_t::string(std::string* str) {
	int16_t size = int16();
	if(size == -1) {
		return false;
	} else if(size < -1) {
		throw exception_t("string size < -1");
	} else {
		str->resize(size);
		raw(size, &(*str)[0]);

		return true;
	}
}

int32_t wire_reader_t::array_size() {
	return int32();
}

std::unique_ptr<io_buff_t> wire_reader_t::raw(int32_t size) {
	if(size + pos_ > buff_->length()) {
		throw exception_t("blob_t wire_reader_t::raw(int32_t) overflow");
	}

	auto raw = buff_->clone_one();
	raw->trim_start(pos_);
	raw->trim_end(buff_->length() - pos_ - size);

	return raw;
}

void wire_reader_t::raw(int32_t size, char* buffer) {
	if(size + pos_ > buff_->length()) {
		throw exception_t("void wire_reader_t::raw(int32_t, char*) overflow");
	}

	memcpy(buffer, buff_->data() + pos_, size);
	pos_ += size;
}

bool wire_reader_t::is_end() const {
	return pos_ == buff_->length();
}

const char* wire_reader_t::ptr() const {
	return (char*)buff_->data() + pos_;
}

void wire_reader_t::skip_bytes() {
	int32_t size = int32();
	if(size == -1) {
		return;
	} else if(size < -1) {
		throw exception_t("bytes size < -1");
	} else {
		skip(size);
	}
}

void wire_reader_t::skip(size_t size) {
	if(pos_ + size > buff_->length()) {
		throw exception_t("overflow in wire_reader_t::skip");
	}

	pos_ += size;
}

size_t wire_reader_t::remaining() const {
	return buff_->length() - pos_;
}

size_t wire_reader_t::pos() const {
	return pos_;
}

void wire_reader_t::jump(size_t pos) {
	pos_ = pos;
}

}} // namespace raptor::kafka
