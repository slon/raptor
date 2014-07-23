#pragma once

#include <stdint.h>

namespace raptor { namespace endian {

#define MAKE_LITTLE(n, u) \
u ## int ## n ## _t little(u ## int ## n ## _t i) { \
	return htole ## n (i); \
}

MAKE_LITTLE(16, );
MAKE_LITTLE(16, u);
MAKE_LITTLE(32, );
MAKE_LITTLE(32, u);
MAKE_LITTLE(64, );
MAKE_LITTLE(64, u);

#undef MAKE_LITTLE

#define MAKE_BIG(n, u) \
u ## int ## n ## _t big(u ## int ## n ## _t i) { \
	return htobe ## n (i); \
}

MAKE_BIG(16, );
MAKE_BIG(16, u);
MAKE_BIG(32, );
MAKE_BIG(32, u);
MAKE_BIG(64, );
MAKE_BIG(64, u);

#undef MAKE_BIG

}} // namespace raptor::endian
