#pragma once

#include <deque>

#include <pd/base/log.H>
#include <pd/base/exception.H>
#include <pd/bq/bq_cond.H>

#include <phantom/pd.H>

namespace phantom {

template<class x_t>
class channel_t {
public:
	channel_t() : is_closed_(false) {}

	bool get(x_t* value) {
		bq_cond_t::handler_t handler(cond_);

		while(!is_closed_ && queue_.empty()) {
			if(!bq_success(handler.wait(NULL))) {
				throw exception_sys_t(log::error, errno, "channel_t::get(): %m");
			}
		}

		if(queue_.empty()) {
			wake_up_next(handler);
			return false;
		} else {
			*value = queue_.front();
			queue_.pop_front();
			wake_up_next(handler);
			return true;
		}
	}

	bool put(const x_t& value) {
		bq_cond_t::handler_t handler(cond_);

		if(!is_closed_) {
			queue_.push_back(value);
		}

		wake_up_next(handler);
		return !is_closed_;
	}

	void close() {
		bq_cond_t::handler_t handler(cond_);

		is_closed_ = true;
		wake_up_next(handler);
	}

	bool is_closed() {
		bq_cond_t::handler_t handler(cond_);
		return is_closed_;
	}

private:
	bq_cond_t cond_;
	
	std::deque<x_t> queue_;
	bool is_closed_;

	void wake_up_next(bq_cond_t::handler_t& handler) {
		if(!queue_.empty() || is_closed_) {
			handler.send();
		}
	}
};

} // namespace phantom
