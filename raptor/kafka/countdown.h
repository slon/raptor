#pragma once

#include <pd/base/log.H>
#include <pd/base/exception.H>
#include <pd/bq/bq_cond.H>

#include <phantom/pd.H>

namespace raptor {

class countdown_t {
public:
	countdown_t() : count(0) {}

	void inc(int i = 1) {
		bq_cond_t::handler_t handler(cond);
		count += i;
	}

	void dec(int i = 1) {
		bq_cond_t::handler_t handler(cond);
		count -= i;

		if(count == 0) {
			handler.send(true);
		}
	}

	void wait() {
		bq_cond_t::handler_t handler(cond);

		while(count != 0) {
			if(!bq_success(handler.wait(NULL))) {
				throw exception_sys_t(log::error, errno, "countdown_t::wait(): %m");
			}
		}
	}

private:
	bq_cond_t cond;
	int count;
};

} // namespace raptor
