#pragma once

#include <stdexcept>

#include <raptor/core/likely.h>
#include <raptor/io/endian.h>
#include <raptor/io/io_buff.h>

namespace raptor {

template<class derived_t, class buff_t>
class cursor_base_t {
public:
	const uint8_t* data() const {
		return crt_buf_->data() + offset_;
	}

	// Space available in the current io_buff_t.  May be 0; use peek() instead which
	// will always point to a non-empty chunk of data or at the end of the
	// chain.
	size_t length() const {
		return crt_buf_->length() - offset_;
	}

	derived_t& operator+=(size_t offset) {
		derived_t* p = static_cast<derived_t*>(this);
		p->skip(offset);
		return *p;
	}

	template <class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type
	read() {
		T val;
		pull(&val, sizeof(T));
		return val;
	}

	template <class T>
	T read_be() {
		return endian::big(read<T>());
	}

	template <class T>
	T read_le() {
		return endian::little(read<T>());
	}

	explicit cursor_base_t(buff_t* buf) :
		crt_buf_(buf),
		offset_(0),
		buffer_(buf) {}

	template <class D, class B> friend class cursor_base_t;

	template <class T>
	explicit cursor_base_t(const T& cursor) {
		crt_buf_ = cursor.crt_buf_;
		offset_ = cursor.offset_;
		buffer_ = cursor.buffer_;
	}

	void reset(buff_t* buf) {
		crt_buf_ = buf;
		buffer_ = buf;
		offset_ = 0;
	}

	std::pair<const uint8_t*, size_t> peek() {
		// Ensure that we're pointing to valid data
		size_t available = length();
		while (UNLIKELY(available == 0 && try_advance_buffer())) {
			available = length();
		}

		return std::make_pair(data(), available);
	}

	void pull(void* buf, size_t len) {
		if (UNLIKELY(pull_at_most(buf, len) != len)) {
			throw std::out_of_range("underflow");
		}
	}

	void clone(std::unique_ptr<io_buff_t>& buff, size_t len) {
		if (UNLIKELY(clone_at_most(buff, len) != len)) {
			throw std::out_of_range("underflow");
		}
	}

	void skip(size_t len) {
		if (UNLIKELY(skip_at_most(len) != len)) {
			throw std::out_of_range("underflow");
		}
	}

	size_t pull_at_most(void* buf, size_t len) {
		uint8_t* p = reinterpret_cast<uint8_t*>(buf);
		size_t copied = 0;
		for (;;) {
			// Fast path: it all fits in one buffer.
			size_t available = length();
			if (LIKELY(available >= len)) {
				memcpy(p, data(), len);
				offset_ += len;
				return copied + len;
			}

			memcpy(p, data(), available);
			copied += available;
			if (UNLIKELY(!try_advance_buffer())) {
				return copied;
			}
			p += available;
			len -= available;
		}
	}

	size_t clone_at_most(std::unique_ptr<io_buff_t>& buf, size_t len) {
		buf.reset(nullptr);

		std::unique_ptr<io_buff_t> tmp;
		size_t copied = 0;
		for (;;) {
			// Fast path: it all fits in one buffer.
			size_t available = length();
			if (LIKELY(available >= len)) {
				tmp = crt_buf_->clone_one();
				tmp->trim_start(offset_);
				tmp->trim_end(tmp->length() - len);
				offset_ += len;
				if (!buf) {
					buf = std::move(tmp);
				} else {
					buf->prepend_chain(std::move(tmp));
				}
				return copied + len;
			}

			tmp = crt_buf_->clone_one();
			tmp->trim_start(offset_);
			if (!buf) {
				buf = std::move(tmp);
			} else {
				buf->prepend_chain(std::move(tmp));
			}

			copied += available;
			if (UNLIKELY(!try_advance_buffer())) {
				return copied;
			}
			len -= available;
		}
	}

	size_t skip_at_most(size_t len) {
		size_t skipped = 0;
		for (;;) {
			// Fast path: it all fits in one buffer.
			size_t available = length();
			if (LIKELY(available >= len)) {
				offset_ += len;
				return skipped + len;
			}

			skipped += available;
			if (UNLIKELY(!try_advance_buffer())) {
				return skipped;
			}
			len -= available;
		}
	}

protected:
	buff_t* crt_buf_;
	size_t offset_;

	bool try_advance_buffer() {
		buff_t* next_buf = crt_buf_->next();
		if (UNLIKELY(next_buf == buffer_)) {
			offset_ = crt_buf_->length();
			return false;
		}

		offset_ = 0;
		crt_buf_ = next_buf;
		return true;
	}

private:
	buff_t* buffer_;
};

template<class derived_t>
class writable_t {
public:
	template<class T>
	typename std::enable_if<std::is_integral<T>::value>::type
	write(T value) {
		const uint8_t* u8 = reinterpret_cast<const uint8_t*>(&value);
		derived_t* d = static_cast<derived_t*>(this);
		d->push(u8, sizeof(T));
	}

	template<class T>
	void write_be(T value) {
		derived_t* d = static_cast<derived_t*>(this);
		d->write(endian::big(value));
	}

	template<class T>
	void write_le(T value) {
		derived_t* d = static_cast<derived_t*>(this);
		d->write(endian::little(value));
	}

	void push(const uint8_t* buf, size_t len) {
		derived_t* d = static_cast<derived_t*>(this);
		if (d->push_at_most(buf, len) != len) {
			throw std::out_of_range("overflow");
		}
	}
};

class cursor_t : public cursor_base_t<cursor_t, const io_buff_t> {
public:
	explicit cursor_t(const io_buff_t* buf)
		: cursor_base_t<cursor_t, const io_buff_t>(buf) {}

	template<class other_cursor_t>
	explicit cursor_t(other_cursor_t& cursor)
		: cursor_base_t<cursor_t, const io_buff_t>(cursor) {}
};

class rw_cursor_t : public cursor_base_t<rw_cursor_t, io_buff_t>, public writable_t<rw_cursor_t> {
public:
	explicit rw_cursor_t(io_buff_t* buf) :
		cursor_base_t<rw_cursor_t, io_buff_t>(buf) {}

	template<class other_cursor_t>
	explicit rw_cursor_t(other_cursor_t& cursor)
		: cursor_base_t<rw_cursor_t, io_buff_t>(cursor) {}

	/**
	 * Gather at least n bytes contiguously into the current buffer,
	 * by coalescing subsequent buffers from the chain as necessary.
	 */
	void gather(size_t n) {
		this->crt_buf_->gather(this->offset_ + n);
	}

	size_t push_at_most(const uint8_t* buf, size_t len) {
		size_t copied = 0;
		for (;;) {
			// Fast path: the current buffer is big enough.
			size_t available = this->length();
			if (LIKELY(available >= len)) {
				memcpy(writable_data(), buf, len);
				this->offset_ += len;
				return copied + len;
			}

			memcpy(writable_data(), buf, available);
			copied += available;
			if (UNLIKELY(!this->try_advance_buffer())) {
				return copied;
			}
			buf += available;
			len -= available;
		}
	}

	void insert(std::unique_ptr<io_buff_t> buf) {
		io_buff_t* next_buf;
		if (this->offset_ == 0) {
			// Can just prepend
			next_buf = buf.get();
			this->crt_buf_->prepend_chain(std::move(buf));
		} else {
			std::unique_ptr<io_buff_t> remaining;
			if (this->crt_buf_->length() - this->offset_ > 0) {
				// Need to split current io_buff_t in two.
				remaining = this->crt_buf_->clone_one();
				remaining->trim_start(this->offset_);
				next_buf = remaining.get();
				buf->prepend_chain(std::move(remaining));
			} else {
				// Can just append
				next_buf = this->crt_buf_->next();
			}
			this->crt_buf_->trim_end(this->length());
			this->crt_buf_->append_chain(std::move(buf));
		}
		// Jump past the new links
		this->offset_ = 0;
		this->crt_buf_ = next_buf;
	}

	uint8_t* writable_data() {
		return this->crt_buf_->writable_data() + this->offset_;
	}
};

class appender_t : public writable_t<appender_t> {
public:
	appender_t(io_buff_t* buf, uint32_t growth) :
		buffer_(buf),
		crt_buf_(buf->prev()),
		growth_(growth) {}

	uint8_t* writable_data() {
		return crt_buf_->writable_tail();
	}

	size_t length() const {
		return crt_buf_->tailroom();
	}

	/**
	 * Mark n bytes (must be <= length()) as appended, as per the
	 * io_buff_t::append() method.
	 */
	void append(size_t n) {
		crt_buf_->append(n);
	}

	/**
	 * Ensure at least n contiguous bytes available to write.
	 * Postcondition: length() >= n.
	 */
	void ensure(uint32_t n) {
		if (LIKELY(length() >= n)) {
			return;
		}

		// Waste the rest of the current buffer and allocate a new one.
		// Don't make it too small, either.
		if (growth_ == 0) {
			throw std::out_of_range("can't grow buffer chain");
		}

		n = std::max(n, growth_);
		buffer_->prepend_chain(io_buff_t::create(n));
		crt_buf_ = buffer_->prev();
	}

	size_t push_at_most(const uint8_t* buf, size_t len) {
		size_t copied = 0;
		for (;;) {
			// Fast path: it all fits in one buffer.
			size_t available = length();
			if (LIKELY(available >= len)) {
				memcpy(writable_data(), buf, len);
				append(len);
				return copied + len;
			}

			memcpy(writable_data(), buf, available);
			append(available);
			copied += available;
			if (UNLIKELY(!try_grow_chain())) {
				return copied;
			}
			buf += available;
			len -= available;
		}
	}

private:
	bool try_grow_chain() {
		assert(crt_buf_->next() == buffer_);
		if (growth_ == 0) {
			return false;
		}

		buffer_->prepend_chain(io_buff_t::create(growth_));
		crt_buf_ = buffer_->prev();
		return true;
	}

	io_buff_t* buffer_;
	io_buff_t* crt_buf_;
	uint32_t growth_;
};

} // namespace raptor
