#include <map>

#include <raptor/core/mutex.h>

#include <raptor/client/channel.h>

namespace raptor {

class caching_channel_factory_t {
public:
	caching_channel_factory_t(channel_factory_ptr_t factory)
		: factory_(factory) {}

	virtual future_t<channel_ptr_t> make_channel(const std::string& address) {
		std::lock_guard<mutex_t> guard(mutex_);

		auto& cache_hit = cache_[address];
		if(cache_hit.is_valid()) {
		    if(!cache_hit.is_ready())
				return cache_hit;

			if(cache_hit.has_value() && cache_hit.get()->is_running())
				return cache_hit;
		}

		cache_hit = factory_->make_channel(address);
		return cache_hit;
	}

	virtual future_t<void> shutdown() {
		std::lock_guard<mutex_t> guard(mutex_);

		std::vector<future_t<void>> shutdown_futures;
		shutdown_futures.push_back(factory_->shutdown());

		for(auto& cached_channel : cache_) {
			shutdown_futures.push_back(cached_channel.second.bind([] (future_t<channel_ptr_t> channel) {
				return channel.get()->shutdown();
			}));
		}

		return when_all(shutdown_futures);
	}

private:
	mutex_t mutex_;

	channel_factory_ptr_t factory_;
	std::map<std::string, future_t<channel_ptr_t>> cache_;
};

} // namespace raptor
