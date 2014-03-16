#pragma once

namespace raptor {

struct no_copy_or_move_t {
	no_copy_or_move_t() {}
	no_copy_or_move_t(const no_copy_or_move_t&) = delete;
	no_copy_or_move_t(no_copy_or_move_t&&) = delete;
	no_copy_or_move_t& operator = (const no_copy_or_move_t&) = delete;
	no_copy_or_move_t& operator = (no_copy_or_move_t&&) = delete;
};

};
