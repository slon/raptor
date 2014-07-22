#include <raptor/io/file_cache.h>

#include <stdexcept>

namespace raptor {

file_cache_t::file_cache_t(size_t size, duration_t check_interval) {
	(void)size;
	(void)check_interval;
}

std::shared_ptr<const file_cache_t::file_t> file_cache_t::open(char const* path) {
	(void)path;
	throw std::runtime_error("not implemented");
}

} // namespace raptor
