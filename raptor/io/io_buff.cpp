#include <raptor/io/io_buff.h>

#include <stdexcept>

namespace raptor {

using std::unique_ptr;

const uint32_t io_buff_t::kMaxIOBufSize;
// Note: Applying offsetof() to an io_buff_t is legal according to C++11, since
// io_buff_t is a standard-layout class.  However, this isn't legal with earlier
// C++ standards, which require that offsetof() only be used with POD types.
const uint32_t io_buff_t::kMaxInternalDataSize = kMaxIOBufSize - static_cast<uint32_t>((uint64_t)&(((io_buff_t*)(0))->int_.buf));

io_buff_t::shared_info_t::shared_info_t() : free_fn(NULL), user_data(NULL) {
	// Use relaxed memory ordering here.  Since we are creating a new shared_info_t,
	// no other threads should be referring to it yet.
	refcount.store(1, std::memory_order_relaxed);
}

io_buff_t::shared_info_t::shared_info_t(free_function_t fn, void* arg) : free_fn(fn), user_data(arg) {
	// Use relaxed memory ordering here.  Since we are creating a new shared_info_t,
	// no other threads should be referring to it yet.
	refcount.store(1, std::memory_order_relaxed);
}

void* io_buff_t::operator new(size_t size) {
	// Since io_buff_t::create() manually allocates space for some io_buff_t objects
	// using malloc(), override operator new so that all io_buff_t objects are
	// always allocated using malloc().  This way operator delete can always know
	// that free() is the correct way to deallocate the memory.
	void* ptr = malloc(size);

	// operator new is not allowed to return NULL
	if (ptr == NULL) {
		throw std::bad_alloc();
	}

	return ptr;
}

void* io_buff_t::operator new(size_t size, void* ptr) {
	assert(size <= kMaxIOBufSize);
	return ptr;
}

void io_buff_t::operator delete(void* ptr) {
	// For small buffers, io_buff_t::create() manually allocates the space for the
	// io_buff_t object using malloc().  Therefore we override delete to ensure that
	// the io_buff_t space is freed using free() rather than a normal delete.
	free(ptr);
}

unique_ptr<io_buff_t> io_buff_t::create(uint32_t capacity) {
	// If the desired capacity is less than kMaxInternalDataSize,
	// just allocate a single region large enough for both the io_buff_t header and
	// the data.
	if (capacity <= kMaxInternalDataSize) {
		void* buf = malloc(kMaxIOBufSize);
		if (buf == NULL) {
			throw std::bad_alloc();
		}

		uint8_t* buf_end = static_cast<uint8_t*>(buf) + kMaxIOBufSize;
		unique_ptr<io_buff_t> iobuf(new(buf) io_buff_t(buf_end));
		assert(iobuf->capacity() >= capacity);
		return iobuf;
	}

	// Allocate an external buffer
	uint8_t* buf;
	shared_info_t* sharedInfo;
	uint32_t actualCapacity;
	alloc_ext_buffer(capacity, &buf, &sharedInfo, &actualCapacity);

	// Allocate the io_buff_t header
	try {
		return unique_ptr<io_buff_t>(new io_buff_t(kExtAllocated, 0, buf,
			actualCapacity, buf, 0, sharedInfo
		));
	} catch (...) {
		free(buf);
		throw;
	}
}

unique_ptr<io_buff_t> io_buff_t::create_chain(size_t total_capacity, uint32_t max_buf_capacity) {
	unique_ptr<io_buff_t> out = create(std::min(total_capacity, size_t(max_buf_capacity)));
	size_t allocated_capacity = out->capacity();

	while (allocated_capacity < total_capacity) {
		unique_ptr<io_buff_t> new_buf = create(std::min(total_capacity - allocated_capacity, size_t(max_buf_capacity)));
		allocated_capacity += new_buf->capacity();
		out->prepend_chain(std::move(new_buf));
	}

	return out;
}

unique_ptr<io_buff_t> io_buff_t::take_ownership(void* buf, uint32_t capacity,
		uint32_t length, free_function_t free_fn, void* user_data, bool free_on_error) {
	shared_info_t* sharedInfo = NULL;
	try {
		sharedInfo = new shared_info_t(free_fn, user_data);

		uint8_t* bufPtr = static_cast<uint8_t*>(buf);
		return unique_ptr<io_buff_t>(new io_buff_t(kExtUserSupplied, kFlagFreeSharedInfo,
			bufPtr, capacity,
			bufPtr, length,
			sharedInfo));
	} catch (...) {
		delete sharedInfo;
		if (free_on_error) {
			if (free_fn) {
				try {
					free_fn(buf, user_data);
				} catch (...) {
					// The user's free function is not allowed to throw.
					abort();
				}
			} else {
				free(buf);
			}
		}
		throw;
	}
}

unique_ptr<io_buff_t> io_buff_t::wrap_buffer(const void* buf, uint32_t capacity) {
	// We cast away the const-ness of the buffer here.
	// This is okay since io_buff_t users must use unshare() to create a copy of
	// this buffer before writing to the buffer.
	uint8_t* bufPtr = static_cast<uint8_t*>(const_cast<void*>(buf));
	return unique_ptr<io_buff_t>(new io_buff_t(kExtUserSupplied, kFlagUserOwned,
		bufPtr, capacity,
		bufPtr, capacity,
		NULL));
}

io_buff_t::io_buff_t(uint8_t* end) : next_(this), prev_(this), data_(int_.buf), length_(0), flags_(0) {
	assert(end - int_.buf == kMaxInternalDataSize);
	assert(end - reinterpret_cast<uint8_t*>(this) == kMaxIOBufSize);
}

io_buff_t::io_buff_t(ExtBufTypeEnum type, uint32_t flags,
			uint8_t* buf, uint32_t capacity,
			uint8_t* data, uint32_t length,
			shared_info_t* sharedInfo) :
		next_(this), prev_(this),
		data_(data), length_(length), flags_(kFlagExt | flags) {
	ext_.capacity = capacity;
	ext_.type = type;
	ext_.buf = buf;
	ext_.sharedInfo = sharedInfo;

	assert(data >= buf);
	assert(data + length <= buf + capacity);
	assert(static_cast<bool>(flags & kFlagUserOwned) == (sharedInfo == NULL));
}

io_buff_t::~io_buff_t() {
	// Destroying an io_buff_t destroys the entire chain.
	// Users of io_buff_t should only explicitly delete the head of any chain.
	// The other elements in the chain will be automatically destroyed.
	while (next_ != this) {
		// Since unlink() returns unique_ptr() and we don't store it,
		// it will automatically delete the unlinked element.
		(void)next_->unlink();
	}

	if (flags_ & kFlagExt) {
		decrement_refcount();
	}
}

bool io_buff_t::empty() const {
	const io_buff_t* current = this;
	do {
		if (current->length() != 0) {
			return false;
		}
		current = current->next_;
	} while (current != this);
	return true;
}

uint32_t io_buff_t::count_chain_elements() const {
	uint32_t numElements = 1;
	for (io_buff_t* current = next_; current != this; current = current->next_) {
		++numElements;
	}
	return numElements;
}

uint64_t io_buff_t::compute_chain_data_length() const {
	uint64_t fullLength = length_;
	for (io_buff_t* current = next_; current != this; current = current->next_) {
		fullLength += current->length_;
	}
	return fullLength;
}

void io_buff_t::prepend_chain(unique_ptr<io_buff_t>&& iobuf) {
	// Take ownership of the specified io_buff_t
	io_buff_t* other = iobuf.release();

	// Remember the pointer to the tail of the other chain
	io_buff_t* otherTail = other->prev_;

	// Hook up prev_->next_ to point at the start of the other chain,
	// and other->prev_ to point at prev_
	prev_->next_ = other;
	other->prev_ = prev_;

	// Hook up otherTail->next_ to point at us,
	// and prev_ to point back at otherTail,
	otherTail->next_ = this;
	prev_ = otherTail;
}

unique_ptr<io_buff_t> io_buff_t::clone() const {
	unique_ptr<io_buff_t> newHead(clone_one());

	for (io_buff_t* current = next_; current != this; current = current->next_) {
		newHead->prepend_chain(current->clone_one());
	}

	return newHead;
}

unique_ptr<io_buff_t> io_buff_t::clone_one() const {
	if (flags_ & kFlagExt) {
		if (ext_.sharedInfo) {
			flags_ |= kFlagMaybeShared;
		}
		unique_ptr<io_buff_t> iobuf(new io_buff_t(static_cast<ExtBufTypeEnum>(ext_.type),
                                      flags_, ext_.buf, ext_.capacity,
                                      data_, length_,
                                      ext_.sharedInfo));
		if (ext_.sharedInfo) {
			ext_.sharedInfo->refcount.fetch_add(1, std::memory_order_acq_rel);
		}
		return iobuf;
	} else {
		// We have an internal data buffer that cannot be shared
		// Allocate a new io_buff_t and copy the data into it.
		unique_ptr<io_buff_t> iobuf(io_buff_t::create(kMaxInternalDataSize));
		assert((iobuf->flags_ & kFlagExt) == 0);
		iobuf->data_ += headroom();
		memcpy(iobuf->data_, data_, length_);
		iobuf->length_ = length_;
		return iobuf;
	}
}

void io_buff_t::unshare_one_slow() {
	// Internal buffers are always unshared, so unshareOneSlow() can only be
	// called for external buffers
	assert(flags_ & kFlagExt);

	// Allocate a new buffer for the data
	uint8_t* buf;
	shared_info_t* sharedInfo;
	uint32_t actualCapacity;
	alloc_ext_buffer(ext_.capacity, &buf, &sharedInfo, &actualCapacity);

	// Copy the data
	// Maintain the same amount of headroom.  Since we maintained the same
	// minimum capacity we also maintain at least the same amount of tailroom.
	uint32_t headlen = headroom();
	memcpy(buf + headlen, data_, length_);

	// Release our reference on the old buffer
	decrement_refcount();
	// Make sure kFlagExt is set, and kFlagUserOwned and kFlagFreeSharedInfo
	// are not set.
	flags_ = kFlagExt;

	// Update the buffer pointers to point to the new buffer
	data_ = buf + headlen;
	ext_.buf = buf;
	ext_.sharedInfo = sharedInfo;
}

void io_buff_t::unshare_chained() {
	// unshareChained() should only be called if we are part of a chain of
	// multiple io_buff_ts.  The caller should have already verified this.
	assert(is_chained());

	io_buff_t* current = this;
	while (true) {
		if (current->is_shared_one()) {
			// we have to unshare
			break;
		}

		current = current->next_;
		if (current == this) {
			// None of the io_buff_ts in the chain are shared,
			// so return without doing anything
			return;
		}
	}

	// We have to unshare.  Let coalesceSlow() do the work.
	coalesce_slow();
}

void io_buff_t::coalesce_slow(size_t maxLength) {
	// coalesceSlow() should only be called if we are part of a chain of multiple
	// io_buff_ts.  The caller should have already verified this.
	assert(is_chained());
	assert(length_ < maxLength);

	// Compute the length of the entire chain
	uint64_t newLength = 0;
	io_buff_t* end = this;
	do {
		newLength += end->length_;
		end = end->next_;
	} while (newLength < maxLength && end != this);

	uint64_t newHeadroom = headroom();
	uint64_t newTailroom = end->prev_->tailroom();
	coalesce_and_reallocate(newHeadroom, newLength, end, newTailroom);
	// We should be only element left in the chain now
	assert(length_ >= maxLength || !is_chained());
}

void io_buff_t::coalesce_and_reallocate(size_t newHeadroom, size_t newLength, io_buff_t* end, size_t newTailroom) {
	uint64_t newCapacity = newLength + newHeadroom + newTailroom;
	if (newCapacity > UINT32_MAX) {
		throw std::overflow_error("io_buff_t chain too large to coalesce");
	}

	// Allocate space for the coalesced buffer.
	// We always convert to an external buffer, even if we happened to be an
	// internal buffer before.
	uint8_t* newBuf;
	shared_info_t* newInfo;
	uint32_t actualCapacity;
	alloc_ext_buffer(newCapacity, &newBuf, &newInfo, &actualCapacity);

	// Copy the data into the new buffer
	uint8_t* newData = newBuf + newHeadroom;
	uint8_t* p = newData;
	io_buff_t* current = this;
	size_t remaining = newLength;
	do {
		assert(current->length_ <= remaining);
		remaining -= current->length_;
		memcpy(p, current->data_, current->length_);
		p += current->length_;
		current = current->next_;
	} while (current != end);
	assert(remaining == 0);

	// Point at the new buffer
	if (flags_ & kFlagExt) {
		decrement_refcount();
	}

	// Make sure kFlagExt is set, and kFlagUserOwned and kFlagFreeSharedInfo
	// are not set.
	flags_ = kFlagExt;

	ext_.capacity = actualCapacity;
	ext_.type = kExtAllocated;
	ext_.buf = newBuf;
	ext_.sharedInfo = newInfo;
	data_ = newData;
	length_ = newLength;

	// Separate from the rest of our chain.
	// Since we don't store the unique_ptr returned by separateChain(),
	// this will immediately delete the returned subchain.
	if (is_chained()) {
		(void)separate_chain(next_, current->prev_);
	}
}

void io_buff_t::decrement_refcount() {
	assert(flags_ & kFlagExt);

	// Externally owned buffers don't have a shared_info_t object and aren't managed
	// by the reference count
	if (flags_ & kFlagUserOwned) {
		assert(ext_.sharedInfo == NULL);
		return;
	}

	// Decrement the refcount
	uint32_t newcnt = ext_.sharedInfo->refcount.fetch_sub(1, std::memory_order_acq_rel);
	// Note that fetch_sub() returns the value before we decremented.
	// If it is 1, we were the only remaining user; if it is greater there are
	// still other users.
	if (newcnt > 1) {
		return;
	}

	// We were the last user.  Free the buffer
	if (ext_.sharedInfo->free_fn != NULL) {
		try {
			ext_.sharedInfo->free_fn(ext_.buf, ext_.sharedInfo->user_data);
		} catch (...) {
			// The user's free function should never throw.  Otherwise we might
			// throw from the io_buff_t destructor.  Other code paths like coalesce()
			// also assume that decrementRefcount() cannot throw.
			abort();
		}
	} else {
		free(ext_.buf);
	}

	// Free the shared_info_t if it was allocated separately.
	//
	// This is only used by takeOwnership().
	//
	// To avoid this special case handling in decrementRefcount(), we could have
	// takeOwnership() set a custom freeFn() that calls the user's free function
	// then frees the shared_info_t object.  (This would require that
	// takeOwnership() store the user's free function with its allocated
	// shared_info_t object.)  However, handling this specially with a flag seems
	// like it shouldn't be problematic.
	if (flags_ & kFlagFreeSharedInfo) {
		delete ext_.sharedInfo;
	}
}

void io_buff_t::reserve_slow(uint32_t minHeadroom, uint32_t minTailroom) {
	size_t newCapacity = (size_t)length_ + minHeadroom + minTailroom;

	// We'll need to reallocate the buffer.
	// There are a few options.
	// - If we have enough total room, move the data around in the buffer
	//   and adjust the data_ pointer.
	// - If we're using an internal buffer, we'll switch to an external
	//   buffer with enough headroom and tailroom.
	// - If we have enough headroom (headroom() >= minHeadroom) but not too much
	//   (so we don't waste memory), we can try one of two things, depending on
	//   whether we use jemalloc or not:
	//   - If using jemalloc, we can try to expand in place, avoiding a memcpy()
	//   - If not using jemalloc and we don't have too much to copy,
	//     we'll use realloc() (note that realloc might have to copy
	//     headroom + data + tailroom, see smartRealloc in folly/Malloc.h)
	// - Otherwise, bite the bullet and reallocate.
	if (headroom() + tailroom() >= minHeadroom + minTailroom) {
		uint8_t* newData = writable_buffer() + minHeadroom;
		memmove(newData, data_, length_);
		data_ = newData;
		return;
	}

	size_t newAllocatedCapacity = good_ext_buffer_size(newCapacity);
	uint8_t* newBuffer = nullptr;
	uint32_t newHeadroom = 0;
	uint32_t oldHeadroom = headroom();

	if ((flags_ & kFlagExt) && length_ != 0 && oldHeadroom >= minHeadroom) {
		size_t copySlack = capacity() - length_;
		if (copySlack * 2 <= length_) {
			void* p = realloc(ext_.buf, newAllocatedCapacity);
			if (p == nullptr) {
				throw std::bad_alloc();
			}
			newBuffer = static_cast<uint8_t*>(p);
			newHeadroom = oldHeadroom;
		}
	}

	// None of the previous reallocation strategies worked (or we're using
	// an internal buffer).  malloc/copy/free.
	if (newBuffer == nullptr) {
		void* p = malloc(newAllocatedCapacity);
		if (p == nullptr) {
			throw std::bad_alloc();
		}
		newBuffer = static_cast<uint8_t*>(p);
		memcpy(newBuffer + minHeadroom, data_, length_);
		if (flags_ & kFlagExt) {
			free(ext_.buf);
		}
		newHeadroom = minHeadroom;
	}

	shared_info_t* info;
	uint32_t cap;
	init_ext_buffer(newBuffer, newAllocatedCapacity, &info, &cap);

	flags_ = kFlagExt;

	ext_.capacity = cap;
	ext_.type = kExtAllocated;
	ext_.buf = newBuffer;
	ext_.sharedInfo = info;
	data_ = newBuffer + newHeadroom;
	// length_ is unchanged
}

void io_buff_t::alloc_ext_buffer(uint32_t minCapacity, uint8_t** bufReturn, shared_info_t** infoReturn, uint32_t* capacityReturn) {
	size_t mallocSize = good_ext_buffer_size(minCapacity);
	uint8_t* buf = static_cast<uint8_t*>(malloc(mallocSize));
	if (buf == NULL) {
		throw std::bad_alloc();
	}
	init_ext_buffer(buf, mallocSize, infoReturn, capacityReturn);
	*bufReturn = buf;
}

size_t io_buff_t::good_ext_buffer_size(uint32_t minCapacity) {
	// Determine how much space we should allocate.  We'll store the shared_info_t
	// for the external buffer just after the buffer itself.  (We store it just
	// after the buffer rather than just before so that the code can still just
	// use free(ext_.buf) to free the buffer.)
	size_t minSize = static_cast<size_t>(minCapacity) + sizeof(shared_info_t);
	// Add room for padding so that the SharedInfo will be aligned on an 8-byte
	// boundary.
	minSize = (minSize + 7) & ~7;

	// TODO
	// Use goodMallocSize() to bump up the capacity to a decent size to request
	// from malloc, so we can use all of the space that malloc will probably give
	// us anyway.
	return minSize;
}

void io_buff_t::init_ext_buffer(uint8_t* buf, size_t mallocSize, shared_info_t** infoReturn, uint32_t* capacityReturn) {
	// Find the shared_info_t storage at the end of the buffer
	// and construct the shared_info_t.
	uint8_t* infoStart = (buf + mallocSize) - sizeof(shared_info_t);
	shared_info_t* sharedInfo = new(infoStart) shared_info_t;

	size_t actualCapacity = infoStart - buf;
	// On the unlikely possibility that the actual capacity is larger than can
	// fit in a uint32_t after adding room for the refcount and calling
	// goodMallocSize(), truncate downwards if necessary.
	if (actualCapacity >= UINT32_MAX) {
		*capacityReturn = UINT32_MAX;
	} else {
		*capacityReturn = actualCapacity;
	}

	*infoReturn = sharedInfo;
}

} // namespace raptor
