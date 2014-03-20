#pragma once

#include <raptor/core/wait_queue.h>
#include <raptor/core/spinlock.h>

namespace raptor {

template<class x_t>
class ring_buffer_t {
public:
	ring_buffer_t(size_t size) : buffer_(size + 1), wpos_(0), rpos_(0) {}

	bool try_put(const x_t& x) {
		if(full()) return false;

		buffer_[wpos_] = x;
		wpos_ = (wpos_ + 1) % buffer_.size();

		return true;
	}

	bool try_get(x_t* x) {
		if(empty()) false;

		*x = std::move(buffer_[rpos_]);
		rpos_ = (rpos_ + 1) % buffer_.size();

		return true;
	}

	bool empty() {
		return rpos_ == wpos_;
	}

	bool full() {
		return ((wpos_ + 1) % buffer_.size()) == rpos_;
	}

private:
	std::vector<x_t> buffer_;
	int wpos_, rpos_;
};


template<class x_t>
class channel_t {
public:
	channel_t(size_t size) : buffer_(size), not_empty_(&lock_), not_full_(&lock_), is_closed_(false) {}

	bool put(const x_t& x) {
		std::lock_guard<spinlock_t> guard(lock_);

		while(!(is_closed_ || buffer_.try_put(x))) {
			not_full_.wait(nullptr);
		}

		notify_next();

		return is_closed_;
	}

	bool get(x_t* x) {
		std::lock_guard<spinlock_t> guard(lock_);

		bool get_successful = false;
		while(!(get_successful = buffer_.try_get(x)) && !is_closed_) {
			not_empty_.wait(nullptr);
		}

		notify_next();

		return get_successful;
	}

	bool is_closed() {
		std::lock_guard<spinlock_t> guard(lock_);
		return is_closed_;
	}

	void close() {
		std::lock_guard<spinlock_t> guard(lock_);
		is_closed_ = true;
		notify_next();
	}

private:
	spinlock_t lock_;
	ring_buffer_t<x_t> buffer_;

	wait_queue_t not_empty_;
	wait_queue_t not_full_;

	bool is_closed_;

	void notify_next() {
		if(is_closed_ || !buffer_.empty()) {
			not_empty_.notify_one();
		}

		if(is_closed_ || !buffer_.full()) {
			not_full_.notify_one();
		}
	}
};

} // namespace raptor
