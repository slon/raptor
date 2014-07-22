#pragma once

#include <raptor/core/wait_queue.h>
#include <raptor/core/spinlock.h>

namespace raptor {

template<class x_t>
class ring_buffer_t {
public:
	ring_buffer_t(size_t size) : buffer_(size + 1), wpos_(0), rpos_(0) {}

	bool try_put(const x_t& x) {
		if(is_full()) return false;

		buffer_[wpos_] = x;
		wpos_ = (wpos_ + 1) % buffer_.size();

		return true;
	}

	bool try_get(x_t* x) {
		if(is_empty()) return false;

		*x = std::move(buffer_[rpos_]);
		rpos_ = (rpos_ + 1) % buffer_.size();

		return true;
	}

	bool is_empty() {
		return rpos_ == wpos_;
	}

	bool is_full() {
		return ((wpos_ + 1) % buffer_.size()) == rpos_;
	}

private:
	std::vector<x_t> buffer_;
	size_t wpos_, rpos_;
};


template<class x_t>
class channel_t {
public:
	channel_t(size_t size) :
		buffer_(size),
		readers_(&lock_),
		writers_(&lock_),
		is_closed_(false),
		wake_up_reader_(false) {}

	bool put(const x_t& x) {
		std::lock_guard<spinlock_t> guard(lock_);

		while(!(is_closed_ || buffer_.try_put(x))) {
			writers_.wait(nullptr);
		}

		notify_next();

		return !is_closed_;
	}

	bool get(x_t* x) {
		std::lock_guard<spinlock_t> guard(lock_);

		bool get_successful = false;
		while(!(get_successful = buffer_.try_get(x)) && !is_closed_ && !wake_up_reader_) {
			readers_.wait(nullptr);
		}

		if(wake_up_reader_) wake_up_reader_ = false;

		notify_next();

		return get_successful;
	}

	void wake_up_reader() {
		std::lock_guard<spinlock_t> guard(lock_);
		wake_up_reader_ = true;
		readers_.notify_one();
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

	wait_queue_t readers_;
	wait_queue_t writers_;

	bool is_closed_;
	bool wake_up_reader_;

	void notify_next() {
		if(is_closed_ || !buffer_.is_empty()) {
			readers_.notify_one();
		}

		if(is_closed_ || !buffer_.is_full()) {
			writers_.notify_one();
		}
	}
};

} // namespace raptor
