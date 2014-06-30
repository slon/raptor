#pragma once

#include <memory>

#include <raptor/core/time.h>
#include <raptor/core/future.h>

namespace raptor {

struct request_t {
	virtual void write(int fd, duration_t* timeout) = 0;

	virtual ~request_t() {}
};

struct response_t {
	virtual void read(int fd, duration_t* timeout) = 0;

	virtual ~response_t() {}
};

struct channel_t {
	virtual future_t<void> send(
		const std::shared_ptr<request_t>& request,
		const std::shared_ptr<response_t>& response
	) = 0;

	virtual bool is_running() = 0;
	virtual future_t<void> shutdown() = 0;

	virtual ~channel_t() {}
};

typedef std::shared_ptr<channel_t> channel_ptr_t;

struct channel_factory_t {
	virtual future_t<channel_ptr_t> make_channel(const std::string& address) = 0;

	virtual future_t<void> shutdown() = 0;

	virtual ~channel_factory_t() {};
};

typedef std::shared_ptr<channel_factory_t> channel_factory_ptr_t;

channel_factory_ptr_t make_cached_channel_factory(channel_factory_ptr_t channel_factory);

} // namespace raptor
