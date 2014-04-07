#pragma once

#include <memory>

#include <raptor/core/time.h>

namespace raptor {

class file_cache_t {
public:
	struct file_t {
		int fd;
	};

	file_cache_t(size_t size, duration_t check_interval = std::chrono::seconds(1));

	std::shared_ptr<const file_t> open(char const* path);
};

} // namespace raptor
