#pragma once

#include <memory>

#include <raptor/kafka/exception.h>

namespace raptor { namespace kafka {

// refcounted, immutable byte buffer
struct blob_t {
public:
	blob_t() : buffer_(NULL), size_(0), start_(0) {}

	blob_t(std::shared_ptr<const char> buffer, size_t size, size_t start = 0)
		: buffer_(buffer), size_(size), start_(start) {}

	size_t size() const { return size_; }
	const char* data() const { return buffer_.get() + start_; }
	bool empty() const { return size_ == 0; }

	blob_t slice(size_t start, size_t size) {
		if(!(start + size <= size_)) {
			throw exception_t("blob_t::slice overflow");
		}

		return blob_t(buffer_, size, start_ + start);
	}

	bool operator == (const blob_t& rhs) const {
		return buffer_ == rhs.buffer_ && size_ == rhs.size_ && start_ == rhs.start_;
	}

private:
	std::shared_ptr<const char> buffer_;
	size_t size_;
	size_t start_;
};

}} // namespace raptor::kafka
