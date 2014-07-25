#pragma once

#include <memory>

namespace raptor {

class io_buff_t;

namespace kafka {

std::unique_ptr<io_buff_t> snappy_decompress(char const* data, size_t data_size);
std::unique_ptr<io_buff_t> snappy_compress(char const* data, size_t data_size);

std::unique_ptr<io_buff_t> xerial_snappy_decompress(io_buff_t const* buf);
std::unique_ptr<io_buff_t> xerial_snappy_compress(io_buff_t const* buf);

}} // namespace raptor::kafka
