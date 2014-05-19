#pragma once

#include <limits>
#include <memory>
#include <string>
#include <atomic>
#include <cassert>

#include <string.h>

namespace raptor {

/**
 * An io_buff_t is a pointer to a buffer of data.
 *
 * io_buff_t objects are intended to be used primarily for networking code, and are
 * modelled somewhat after FreeBSD's mbuf data structure, and Linux's sk_buff
 * structure.
 *
 * io_buff_t objects facilitate zero-copy network programming, by allowing multiple
 * io_buff_t objects to point to the same underlying buffer of data, using a
 * reference count to track when the buffer is no longer needed and can be
 * freed.
 *
 *
 * Data Layout
 * -----------
 *
 * The io_buff_t itself is a small object containing a pointer to the buffer and
 * information about which segment of the buffer contains valid data.
 *
 * The data layout looks like this:
 *
 *  +-----------+
 *  | io_buff_t |
 *  +-----------+
 *   /
 *  |
 *  v
 *  +------------+--------------------+-----------+
 *  | headroom   |        data        |  tailroom |
 *  +------------+--------------------+-----------+
 *  ^            ^                    ^           ^
 *  buffer()   data()               tail()      bufferEnd()
 *
 *  The length() method returns the length of the valid data; capacity()
 *  returns the entire capacity of the buffer (from buffer() to bufferEnd()).
 *  The headroom() and tailroom() methods return the amount of unused capacity
 *  available before and after the data.
 *
 *
 * Buffer Sharing
 * --------------
 *
 * The buffer itself is reference counted, and multiple io_buff_t objects may point
 * to the same buffer.  Each io_buff_t may point to a different section of valid
 * data within the underlying buffer.  For example, if multiple protocol
 * requests are read from the network into a single buffer, a separate io_buff_t
 * may be created for each request, all sharing the same underlying buffer.
 *
 * In other words, when multiple io_buff_ts share the same underlying buffer, the
 * data() and tail() methods on each io_buff_t may point to a different segment of
 * the data.  However, the buffer() and bufferEnd() methods will point to the
 * same location for all io_buff_ts sharing the same underlying buffer.
 *
 *       +-------------+   +-------------+
 *       | io_buff_t 1 |   | io_buff_t 2 |
 *       +-------------+   +-------------+
 *        |         | _____/        |
 *   data |    tail |/    data      | tail
 *        v         v               v
 *  +-------------------------------------+
 *  |     |         |               |     |
 *  +-------------------------------------+
 *
 * If you only read data from an io_buff_t, you don't need to worry about other
 * io_buff_t objects possibly sharing the same underlying buffer.  However, if you
 * ever write to the buffer you need to first ensure that no other io_buff_ts point
 * to the same buffer.  The unshare() method may be used to ensure that you
 * have an unshared buffer.
 *
 *
 * io_buff_t Chains
 * ------------
 *
 * io_buff_t objects also contain pointers to next and previous io_buff_t objects.
 * This can be used to represent a single logical piece of data that its stored
 * in non-contiguous chunks in separate buffers.
 *
 * A single io_buff_t object can only belong to one chain at a time.
 *
 * io_buff_t chains are always circular.  The "prev" pointer in the head of the
 * chain points to the tail of the chain.  However, it is up to the user to
 * decide which io_buff_t is the head.  Internally the io_buff_t code does not care
 * which element is the head.
 *
 * The lifetime of all io_buff_ts in the chain are linked: when one element in the
 * chain is deleted, all other chained elements are also deleted.  Conceptually
 * it is simplest to treat this as if the head of the chain owns all other
 * io_buff_ts in the chain.  When you delete the head of the chain, it will delete
 * the other elements as well.  For this reason, prependChain() and
 * appendChain() take ownership of of the new elements being added to this
 * chain.
 *
 * When the coalesce() method is used to coalesce an entire io_buff_t chain into a
 * single io_buff_t, all other io_buff_ts in the chain are eliminated and automatically
 * deleted.  The unshare() method may coalesce the chain; if it does it will
 * similarly delete all io_buff_ts eliminated from the chain.
 *
 * As discussed in the following section, it is up to the user to maintain a
 * lock around the entire io_buff_t chain if multiple threads need to access the
 * chain.  io_buff_t does not provide any internal locking.
 *
 *
 * Synchronization
 * ---------------
 *
 * When used in multithread programs, a single io_buff_t object should only be used
 * in a single thread at a time.  If a caller uses a single io_buff_t across
 * multiple threads the caller is responsible for using an external lock to
 * synchronize access to the io_buff_t.
 *
 * Two separate io_buff_t objects may be accessed concurrently in separate threads
 * without locking, even if they point to the same underlying buffer.  The
 * buffer reference count is always accessed atomically, and no other
 * operations should affect other io_buff_ts that point to the same data segment.
 * The caller is responsible for using unshare() to ensure that the data buffer
 * is not shared by other io_buff_ts before writing to it, and this ensures that
 * the data itself is not modified in one thread while also being accessed from
 * another thread.
 *
 * For io_buff_t chains, no two io_buff_ts in the same chain should be accessed
 * simultaneously in separate threads.  The caller must maintain a lock around
 * the entire chain if the chain, or individual io_buff_ts in the chain, may be
 * accessed by multiple threads.
 *
 *
 * io_buff_t Object Allocation/Sharing
 * -------------------------------
 *
 * io_buff_t objects themselves are always allocated on the heap.  The io_buff_t
 * constructors are private, so io_buff_t objects may not be created on the stack.
 * In part this is done since some io_buff_t objects use small-buffer optimization
 * and contain the buffer data immediately after the io_buff_t object itself.  The
 * coalesce() and unshare() methods also expect to be able to delete subsequent
 * io_buff_t objects in the chain if they are no longer needed due to coalescing.
 *
 * The io_buff_t structure also does not provide room for an intrusive refcount on
 * the io_buff_t object itself, only the underlying data buffer is reference
 * counted.  If users want to share the same io_buff_t object between multiple
 * parts of the code, they are responsible for managing this sharing on their
 * own.  (For example, by using a shared_ptr.  Alternatively, users always have
 * the option of using clone() to create a second io_buff_t that points to the same
 * underlying buffer.)
 *
 * With jemalloc, allocating small objects like io_buff_t objects should be
 * relatively fast, and the cost of allocating io_buff_t objects on the heap and
 * cloning new io_buff_ts should be relatively cheap.
 */

class io_buff_t {
public:
	typedef void (*free_function_t)(void* buf, void* user_data);

	/**
	 * Allocate a new io_buff_t object with the requested capacity.
	 *
	 * Returns a new io_buff_t object that must be (eventually) deleted by the
	 * caller.  The returned io_buff_t may actually have slightly more capacity than
	 * requested.
	 *
	 * The data pointer will initially point to the start of the newly allocated
	 * buffer, and will have a data length of 0.
	 *
	 * Throws std::bad_alloc on error.
	 */
	static std::unique_ptr<io_buff_t> create(uint32_t capacity);

	/**
	 * Allocate a new io_buff_t chain with the requested total capacity, allocating
	 * no more than maxBufCapacity to each buffer.
	 */
	static std::unique_ptr<io_buff_t> create_chain(size_t total_capacity, uint32_t max_buf_capacity);

	/**
	 * Create a new io_buff_t pointing to an existing data buffer.
	 *
	 * The new io_buff_tfer will assume ownership of the buffer, and free it by
	 * calling the specified FreeFunction when the last io_buff_t pointing to this
	 * buffer is destroyed.  The function will be called with a pointer to the
	 * buffer as the first argument, and the supplied userData value as the
	 * second argument.  The free function must never throw exceptions.
	 *
	 * If no FreeFunction is specified, the buffer will be freed using free()
	 * which will result in undefined behavior if the memory was allocated
	 * using 'new'.
	 *
	 * The io_buff_t data pointer will initially point to the start of the buffer,
	 *
	 * In the first version of this function, the length of data is unspecified
	 * and is initialized to the capacity of the buffer
	 *
	 * In the second version, the user specifies the valid length of data
	 * in the buffer
	 *
	 * On error, std::bad_alloc will be thrown.  If freeOnError is true (the
	 * default) the buffer will be freed before throwing the error.
	 */
	static std::unique_ptr<io_buff_t> take_ownership(void* buf, uint32_t capacity,
			free_function_t free_fn = NULL, void* user_data = NULL, bool free_on_error = true) {
		return take_ownership(buf, capacity, capacity, free_fn, user_data, free_on_error);
	}

	static std::unique_ptr<io_buff_t> take_ownership(void* buf, uint32_t capacity, uint32_t length,
			free_function_t free_fn = NULL, void* user_data = NULL, bool free_on_error = true);

	/**
	 * Create a new io_buff_t pointing to an existing data buffer made up of
	 * count objects of a given standard-layout type.
	 *
	 * This is dangerous -- it is essentially equivalent to doing
	 * reinterpret_cast<unsigned char*> on your data -- but it's often useful
	 * for serialization / deserialization.
	 *
	 * The new io_buff_tfer will assume ownership of the buffer, and free it
	 * appropriately (by calling the UniquePtr's custom deleter, or by calling
	 * delete or delete[] appropriately if there is no custom deleter)
	 * when the buffer is destroyed.  The custom deleter, if any, must never
	 * throw exceptions.
	 *
	 * The io_buff_t data pointer will initially point to the start of the buffer,
	 * and the length will be the full capacity of the buffer (count *
	 * sizeof(T)).
	 *
	 * On error, std::bad_alloc will be thrown, and the buffer will be freed
	 * before throwing the error.
	 */
	template <class unique_ptr_t>
	static std::unique_ptr<io_buff_t> take_ownership(unique_ptr_t&& buf, size_t count=1);

	/**
	 * Create a new io_buff_t object that points to an existing user-owned buffer.
	 *
	 * This should only be used when the caller knows the lifetime of the io_buff_t
	 * object ahead of time and can ensure that all io_buff_t objects that will point
	 * to this buffer will be destroyed before the buffer itself is destroyed.
	 *
	 * This buffer will not be freed automatically when the last io_buff_t
	 * referencing it is destroyed.  It is the caller's responsibility to free
	 * the buffer after the last io_buff_t has been destroyed.
	 *
	 * The io_buff_t data pointer will initially point to the start of the buffer,
	 * and the length will be the full capacity of the buffer.
	 *
	 * An io_buff_t created using wrapBuffer() will always be reported as shared.
	 * unshare() may be used to create a writable copy of the buffer.
	 *
	 * On error, std::bad_alloc will be thrown.
	 */
	static std::unique_ptr<io_buff_t> wrap_buffer(const void* buf, uint32_t capacity);

	/**
	 * Convenience function to create a new io_buff_t object that copies data from a
	 * user-supplied buffer, optionally allocating a given amount of
	 * headroom and tailroom.
	 */
	static std::unique_ptr<io_buff_t> copy_buffer(const void* buf, uint32_t size, uint32_t headroom=0, uint32_t min_tailroom=0);

	/**
	 * Convenience function to create a new io_buff_t object that copies data from a
	 * user-supplied string, optionally allocating a given amount of
	 * headroom and tailroom.
	 *
	 * Beware when attempting to invoke this function with a constant string
	 * literal and a headroom argument: you will likely end up invoking the
	 * version of copyBuffer() above.  io_buff_t::copyBuffer("hello", 3) will treat
	 * the first argument as a const void*, and will invoke the version of
	 * copyBuffer() above, with the size argument of 3.
	 */
	static std::unique_ptr<io_buff_t> copy_buffer(const std::string& buf, uint32_t headroom=0, uint32_t min_tailroom=0);

	/**
	 * A version of copyBuffer() that returns a null pointer if the input string
	 * is empty.
	 */
	static std::unique_ptr<io_buff_t> maybe_copy_buffer(const std::string& buf, uint32_t headroom=0, uint32_t min_tailroom=0);

	/**
	 * Convenience function to free a chain of io_buff_ts held by a unique_ptr.
	 */
	static void destroy(std::unique_ptr<io_buff_t>&& data) {
		auto destroyer = std::move(data);
	}

	/**
	 * Destroy this io_buff_t.
	 *
	 * Deleting an io_buff_t will automatically destroy all io_buff_ts in the chain.
	 * (See the comments above regarding the ownership model of io_buff_t chains.
	 * All subsequent io_buff_ts in the chain are considered to be owned by the head
	 * of the chain.  Users should only explicitly delete the head of a chain.)
	 *
	 * When each individual io_buff_t is destroyed, it will release its reference
	 * count on the underlying buffer.  If it was the last user of the buffer,
	 * the buffer will be freed.
	 */
	~io_buff_t();

	/**
	 * Check whether the chain is empty (i.e., whether the io_buff_ts in the
	 * chain have a total data length of zero).
	 *
	 * This method is semantically equivalent to
	 *   i->computeChainDataLength()==0
	 * but may run faster because it can short-circuit as soon as it
	 * encounters a buffer with length()!=0
	 */
	bool empty() const;

	/**
	 * Get the pointer to the start of the data.
	 */
	const uint8_t* data() const {
		return data_;
	}

	/**
	 * Get a writable pointer to the start of the data.
	 *
	 * The caller is responsible for calling unshare() first to ensure that it is
	 * actually safe to write to the buffer.
	 */
	uint8_t* writable_data() {
		return data_;
	}

	/**
	 * Get the pointer to the end of the data.
	 */
	const uint8_t* tail() const {
		return data_ + length_;
	}

	/**
	 * Get a writable pointer to the end of the data.
	 *
	 * The caller is responsible for calling unshare() first to ensure that it is
	 * actually safe to write to the buffer.
	 */
	uint8_t* writable_tail() {
		return data_ + length_;
	}

	/**
	 * Get the data length.
	 */
	uint32_t length() const {
		return length_;
	}

	/**
	 * Get the amount of head room.
	 *
	 * Returns the number of bytes in the buffer before the start of the data.
	 */
	uint32_t headroom() const {
		return data_ - buffer();
	}

	/**
	 * Get the amount of tail room.
	 *
	 * Returns the number of bytes in the buffer after the end of the data.
	 */
	uint32_t tailroom() const {
		return buffer_end() - tail();
	}

	/**
	 * Get the pointer to the start of the buffer.
	 *
	 * Note that this is the pointer to the very beginning of the usable buffer,
	 * not the start of valid data within the buffer.  Use the data() method to
	 * get a pointer to the start of the data within the buffer.
	 */
	const uint8_t* buffer() const {
		return (flags_ & kFlagExt) ? ext_.buf : int_.buf;
	}

	/**
	 * Get a writable pointer to the start of the buffer.
	 *
	 * The caller is responsible for calling unshare() first to ensure that it is
	 * actually safe to write to the buffer.
	 */
	uint8_t* writable_buffer() {
		return (flags_ & kFlagExt) ? ext_.buf : int_.buf;
	}

	/**
	 * Get the pointer to the end of the buffer.
	 *
	 * Note that this is the pointer to the very end of the usable buffer,
	 * not the end of valid data within the buffer.  Use the tail() method to
	 * get a pointer to the end of the data within the buffer.
	 */
	const uint8_t* buffer_end() const {
		return (flags_ & kFlagExt) ? ext_.buf + ext_.capacity : int_.buf + kMaxInternalDataSize;
	}

	/**
	 * Get the total size of the buffer.
	 *
	 * This returns the total usable length of the buffer.  Use the length()
	 * method to get the length of the actual valid data in this io_buff_t.
	 */
	uint32_t capacity() const {
		return (flags_ & kFlagExt) ?  ext_.capacity : kMaxInternalDataSize;
	}

	/**
	 * Get a pointer to the next io_buff_t in this chain.
	 */
	io_buff_t* next() { return next_; }
	const io_buff_t* next() const { return next_; }

	/**
	 * Get a pointer to the previous io_buff_t in this chain.
	 */
	io_buff_t* prev() { return prev_; }
	const io_buff_t* prev() const { return prev_; }

	/**
	 * Shift the data forwards in the buffer.
	 *
	 * This shifts the data pointer forwards in the buffer to increase the
	 * headroom.  This is commonly used to increase the headroom in a newly
	 * allocated buffer.
	 *
	 * The caller is responsible for ensuring that there is sufficient
	 * tailroom in the buffer before calling advance().
	 *
	 * If there is a non-zero data length, advance() will use memmove() to shift
	 * the data forwards in the buffer.  In this case, the caller is responsible
	 * for making sure the buffer is unshared, so it will not affect other io_buff_ts
	 * that may be sharing the same underlying buffer.
	 */
	void advance(uint32_t amount) {
		assert(amount <= tailroom());

		if (length_ > 0) {
			memmove(data_ + amount, data_, length_);
		}
		data_ += amount;
	}

	/**
	 * Shift the data backwards in the buffer.
	 *
	 * The caller is responsible for ensuring that there is sufficient headroom
	 * in the buffer before calling retreat().
	 *
	 * If there is a non-zero data length, retreat() will use memmove() to shift
	 * the data backwards in the buffer.  In this case, the caller is responsible
	 * for making sure the buffer is unshared, so it will not affect other io_buff_ts
	 * that may be sharing the same underlying buffer.
	 */
	void retreat(uint32_t amount) {
		assert(amount <= headroom());

		if (length_ > 0) {
			memmove(data_ - amount, data_, length_);
		}
		data_ -= amount;
	}

	/**
	 * Adjust the data pointer to include more valid data at the beginning.
	 *
	 * This moves the data pointer backwards to include more of the available
	 * buffer.  The caller is responsible for ensuring that there is sufficient
	 * headroom for the new data.  The caller is also responsible for populating
	 * this section with valid data.
	 *
	 * This does not modify any actual data in the buffer.
	 */
	void prepend(uint32_t amount) {
		assert(amount <= headroom());
		data_ -= amount;
		length_ += amount;
	}

	/**
	 * Adjust the tail pointer to include more valid data at the end.
	 *
	 * This moves the tail pointer forwards to include more of the available
	 * buffer.  The caller is responsible for ensuring that there is sufficient
	 * tailroom for the new data.  The caller is also responsible for populating
	 * this section with valid data.
	 *
	 * This does not modify any actual data in the buffer.
	 */
	void append(uint32_t amount) {
		assert(amount <= tailroom());
		length_ += amount;
	}

	/**
	 * Adjust the data pointer forwards to include less valid data.
	 *
	 * This moves the data pointer forwards so that the first amount bytes are no
	 * longer considered valid data.  The caller is responsible for ensuring that
	 * amount is less than or equal to the actual data length.
	 *
	 * This does not modify any actual data in the buffer.
	 */
	void trim_start(uint32_t amount) {
		assert(amount <= length_);
		data_ += amount;
		length_ -= amount;
	}

	/**
	 * Adjust the tail pointer backwards to include less valid data.
	 *
	 * This moves the tail pointer backwards so that the last amount bytes are no
	 * longer considered valid data.  The caller is responsible for ensuring that
	 * amount is less than or equal to the actual data length.
	 *
	 * This does not modify any actual data in the buffer.
	 */
	void trim_end(uint32_t amount) {
		assert(amount <= length_);
		length_ -= amount;
	}

	/**
	 * Clear the buffer.
	 *
	 * Postcondition: headroom() == 0, length() == 0, tailroom() == capacity()
	 */
	void clear() {
		data_ = writable_buffer();
		length_ = 0;
	}

	/**
	 * Ensure that this buffer has at least minHeadroom headroom bytes and at
	 * least minTailroom tailroom bytes.  The buffer must be writable
	 * (you must call unshare() before this, if necessary).
	 *
	 * Postcondition: headroom() >= minHeadroom, tailroom() >= minTailroom,
	 * the data (between data() and data() + length()) is preserved.
	 */
	void reserve(uint32_t min_headroom, uint32_t min_tailroom) {
		// Maybe we don't need to do anything.
		if (headroom() >= min_headroom && tailroom() >= min_tailroom) {
			return;
		}
		// If the buffer is empty but we have enough total room (head + tail),
		// move the data_ pointer around.
		if (length() == 0 && headroom() + tailroom() >= min_headroom + min_tailroom) {
			data_ = writable_buffer() + min_headroom;
			return;
		}
		// Bah, we have to do actual work.
		reserve_slow(min_headroom, min_tailroom);
	}

	/**
	 * Return true if this io_buff_t is part of a chain of multiple io_buff_ts, or false
	 * if this is the only io_buff_t in its chain.
	 */
	bool is_chained() const {
		assert((next_ == this) == (prev_ == this));
		return next_ != this;
	}

	/**
	 * Get the number of io_buff_ts in this chain.
	 *
	 * Beware that this method has to walk the entire chain.
	 * Use isChained() if you just want to check if this io_buff_t is part of a chain
	 * or not.
	 */
	uint32_t count_chain_elements() const;

	/**
	 * Get the length of all the data in this io_buff_t chain.
	 *
	 * Beware that this method has to walk the entire chain.
	 */
	uint64_t compute_chain_data_length() const;

	/**
	 * Insert another io_buff_t chain immediately before this io_buff_t.
	 *
	 * For example, if there are two io_buff_t chains (A, B, C) and (D, E, F),
	 * and B->prependChain(D) is called, the (D, E, F) chain will be subsumed
	 * and become part of the chain starting at A, which will now look like
	 * (A, D, E, F, B, C)
	 *
	 * Note that since io_buff_t chains are circular, head->prependChain(other) can
	 * be used to append the other chain at the very end of the chain pointed to
	 * by head.  For example, if there are two io_buff_t chains (A, B, C) and
	 * (D, E, F), and A->prependChain(D) is called, the chain starting at A will
	 * now consist of (A, B, C, D, E, F)
	 *
	 * The elements in the specified io_buff_t chain will become part of this chain,
	 * and will be owned by the head of this chain.  When this chain is
	 * destroyed, all elements in the supplied chain will also be destroyed.
	 *
	 * For this reason, append_chain() only accepts an rvalue-reference to a
	 * unique_ptr(), to make it clear that it is taking ownership of the supplied
	 * chain.  If you have a raw pointer, you can pass in a new temporary
	 * unique_ptr around the raw pointer.  If you have an existing,
	 * non-temporary unique_ptr, you must call std::move(ptr) to make it clear
	 * that you are destroying the original pointer.
	 */
	void prepend_chain(std::unique_ptr<io_buff_t>&& iobuf);

	/**
	 * Append another io_buff_t chain immediately after this io_buff_t.
	 *
	 * For example, if there are two io_buff_t chains (A, B, C) and (D, E, F),
	 * and B->appendChain(D) is called, the (D, E, F) chain will be subsumed
	 * and become part of the chain starting at A, which will now look like
	 * (A, B, D, E, F, C)
	 *
	 * The elements in the specified io_buff_t chain will become part of this chain,
	 * and will be owned by the head of this chain.  When this chain is
	 * destroyed, all elements in the supplied chain will also be destroyed.
	 *
	 * For this reason, appendChain() only accepts an rvalue-reference to a
	 * unique_ptr(), to make it clear that it is taking ownership of the supplied
	 * chain.  If you have a raw pointer, you can pass in a new temporary
	 * unique_ptr around the raw pointer.  If you have an existing,
	 * non-temporary unique_ptr, you must call std::move(ptr) to make it clear
	 * that you are destroying the original pointer.
	 */
	void append_chain(std::unique_ptr<io_buff_t>&& iobuf) {
		// Just use prepend_chain() on the next element in our chain
		next_->prepend_chain(std::move(iobuf));
	}

	/**
	 * Remove this io_buff_t from its current chain.
	 *
	 * Since ownership of all elements an io_buff_t chain is normally maintained by
	 * the head of the chain, unlink() transfers ownership of this io_buff_t from the
	 * chain and gives it to the caller.  A new unique_ptr to the io_buff_t is
	 * returned to the caller.  The caller must store the returned unique_ptr (or
	 * call release() on it) to take ownership, otherwise the io_buff_t will be
	 * immediately destroyed.
	 *
	 * Since unlink transfers ownership of the io_buff_t to the caller, be careful
	 * not to call unlink() on the head of a chain if you already maintain
	 * ownership on the head of the chain via other means.  The pop() method
	 * is a better choice for that situation.
	 */
	std::unique_ptr<io_buff_t> unlink() {
		next_->prev_ = prev_;
		prev_->next_ = next_;
		prev_ = this;
		next_ = this;
		return std::unique_ptr<io_buff_t>(this);
	}

	/**
	 * Remove this io_buff_t from its current chain and return a unique_ptr to
	 * the io_buff_t that formerly followed it in the chain.
	 */
	std::unique_ptr<io_buff_t> pop() {
		io_buff_t *next = next_;
		next_->prev_ = prev_;
		prev_->next_ = next_;
		prev_ = this;
		next_ = this;
		return std::unique_ptr<io_buff_t>((next == this) ? NULL : next);
	}

	/**
	 * Remove a subchain from this chain.
	 *
	 * Remove the subchain starting at head and ending at tail from this chain.
	 *
	 * Returns a unique_ptr pointing to head.  (In other words, ownership of the
	 * head of the subchain is transferred to the caller.)  If the caller ignores
	 * the return value and lets the unique_ptr be destroyed, the subchain will
	 * be immediately destroyed.
	 *
	 * The subchain referenced by the specified head and tail must be part of the
	 * same chain as the current io_buff_t, but must not contain the current io_buff_t.
	 * However, the specified head and tail may be equal to each other (i.e.,
	 * they may be a subchain of length 1).
	 */
	std::unique_ptr<io_buff_t> separate_chain(io_buff_t* head, io_buff_t* tail) {
		assert(head != this);
		assert(tail != this);

		head->prev_->next_ = tail->next_;
		tail->next_->prev_ = head->prev_;

		head->prev_ = tail;
		tail->next_ = head;

		return std::unique_ptr<io_buff_t>(head);
	}

	/**
	 * Return true if at least one of the io_buff_ts in this chain are shared,
	 * or false if all of the io_buff_ts point to unique buffers.
	 *
	 * Use is_shared_one() to only check this io_buff_t rather than the entire chain.
	 */
	bool is_shared() const {
		const io_buff_t* current = this;
		while (true) {
			if (current->is_shared_one()) {
				return true;
			}
			current = current->next_;
			if (current == this) {
				return false;
			}
		}
	}

	/**
	 * Return true if other io_buff_ts are also pointing to the buffer used by this
	 * io_buff_t, and false otherwise.
	 *
	 * If this io_buff_t points at a buffer owned by another (non-io_buff_t) part of the
	 * code (i.e., if the io_buff_t was created using wrap_buffer(), or was cloned
	 * from such an io_buff_t), it is always considered shared.
	 *
	 * This only checks the current io_buff_t, and not other io_buff_ts in the chain.
	 */
	bool is_shared_one() const {
		if (flags_ & (kFlagUserOwned | kFlagMaybeShared) == 0) {
			return false;
		}

		// If this is a user-owned buffer, it is always considered shared
		if (flags_ & kFlagUserOwned) {
			return true;
		}

		// an internal buffer wouldn't have kFlagMaybeShared or kFlagUserOwned
		// so we would have returned false already.  The only remaining case
		// is an external buffer which may be shared, so we need to read
		// the reference count.
		assert((flags_ & (kFlagExt | kFlagMaybeShared)) == (kFlagExt | kFlagMaybeShared));

		bool shared = ext_.sharedInfo->refcount.load(std::memory_order_acquire) > 1;
		if (!shared) {
			// we're the last one left
			flags_ &= ~kFlagMaybeShared;
		}
		return shared;
	}

	/**
	 * Ensure that this io_buff_t has a unique buffer that is not shared by other
	 * io_buff_ts.
	 *
	 * unshare() operates on an entire chain of io_buff_t objects.  If the chain is
	 * shared, it may also coalesce the chain when making it unique.  If the
	 * chain is coalesced, subsequent io_buff_t objects in the current chain will be
	 * automatically deleted.
	 *
	 * Note that buffers owned by other (non-io_buff_t) users are automatically
	 * considered shared.
	 *
	 * Throws std::bad_alloc on error.  On error the io_buff_t chain will be
	 * unmodified.
	 *
	 * Currently unshare may also throw std::overflow_error if it tries to
	 * coalesce.  (TODO: In the future it would be nice if unshare() were smart
	 * enough not to coalesce the entire buffer if the data is too large.
	 * However, in practice this seems unlikely to become an issue.)
	 */
	void unshare() {
		if (is_chained()) {
			unshare_chained();
		} else {
			unshare_one();
		}
	}

	/**
	 * Ensure that this io_buff_t has a unique buffer that is not shared by other
	 * io_buff_ts.
	 *
	 * unshareOne() operates on a single io_buff_t object.  This io_buff_t will have a
	 * unique buffer after unshareOne() returns, but other io_buff_ts in the chain
	 * may still be shared after unshareOne() returns.
	 *
	 * Throws std::bad_alloc on error.  On error the io_buff_t will be unmodified.
	 */
	void unshare_one() {
		if (is_shared_one()) {
			unshare_one_slow();
		}
	}

	/**
	 * Coalesce this io_buff_t chain into a single buffer.
	 *
	 * This method moves all of the data in this io_buff_t chain into a single
	 * contiguous buffer, if it is not already in one buffer.  After coalesce()
	 * returns, this io_buff_t will be a chain of length one.  Other io_buff_ts in the
	 * chain will be automatically deleted.
	 *
	 * After coalescing, the io_buff_t will have at least as much headroom as the
	 * first io_buff_t in the chain, and at least as much tailroom as the last io_buff_t
	 * in the chain.
	 *
	 * Throws std::bad_alloc on error.  On error the io_buff_t chain will be
	 * unmodified.  Throws std::overflow_error if the length of the entire chain
	 * larger than can be described by a uint32_t capacity.
	 *
	 * Returns ByteRange that points to the data io_buff_t stores.
	 */
	// ByteRange coalesce() {
	// 	if (is_chained()) {
	// 		coalesce_slow();
	// 	}
	// 	return ByteRange(data_, length_);
	// }

	/**
	 * Ensure that this chain has at least maxLength bytes available as a
	 * contiguous memory range.
	 *
	 * This method coalesces whole buffers in the chain into this buffer as
	 * necessary until this buffer's length() is at least maxLength.
	 *
	 * After coalescing, the io_buff_t will have at least as much headroom as the
	 * first io_buff_t in the chain, and at least as much tailroom as the last io_buff_t
	 * that was coalesced.
	 *
	 * Throws std::bad_alloc on error.  On error the io_buff_t chain will be
	 * unmodified.  Throws std::overflow_error if the length of the coalesced
	 * portion of the chain is larger than can be described by a uint32_t
	 * capacity.  (Although maxLength is uint32_t, gather() doesn't split
	 * buffers, so coalescing whole buffers may result in a capacity that can't
	 * be described in uint32_t.
	 *
	 * Upon return, either enough of the chain was coalesced into a contiguous
	 * region, or the entire chain was coalesced.  That is,
	 * length() >= maxLength || !isChained() is true.
	 */
	void gather(uint32_t max_length) {
		if (!is_chained() || length_ >= max_length) {
			return;
		}
		coalesce_slow(max_length);
	}

	/**
	 * Return a new io_buff_t chain sharing the same data as this chain.
	 *
	 * The new io_buff_t chain will normally point to the same underlying data
	 * buffers as the original chain.  (The one exception to this is if some of
	 * the io_buff_ts in this chain contain small internal data buffers which cannot
	 * be shared.)
	 */
	std::unique_ptr<io_buff_t> clone() const;

	/**
	 * Return a new io_buff_t with the same data as this io_buff_t.
	 *
	 * The new io_buff_t returned will not be part of a chain (even if this io_buff_t is
	 * part of a larger chain).
	 */
	std::unique_ptr<io_buff_t> clone_one() const;

	/**
	 * Return an iovector suitable for e.g. writev()
	 *
	 *   auto iov = buf->getIov();
	 *   auto xfer = writev(fd, iov.data(), iov.size());
	 *
	 * Naturally, the returned iovector is invalid if you modify the buffer
	 * chain.
	 */
	// std::vector<struct iovec> getIov() const;

	// Overridden operator new and delete.
	// These directly use malloc() and free() to allocate the space for io_buff_t
	// objects.  This is needed since io_buff_t::create() manually uses malloc when
	// allocating io_buff_t objects with an internal buffer.
	void* operator new(size_t size);
	void* operator new(size_t size, void* ptr);
	void operator delete(void* ptr);

private:
	enum FlagsEnum : uint32_t {
		kFlagExt = 0x1,
		kFlagUserOwned = 0x2,
		kFlagFreeSharedInfo = 0x4,
		kFlagMaybeShared = 0x8,
	};

	// Values for the ExternalBuf type field.
	// We currently don't really use this for anything, other than to have it
	// around for debugging purposes.  We store it at the moment just because we
	// have the 4 extra bytes in the ExternalBuf struct that would just be
	// padding otherwise.
	enum ExtBufTypeEnum {
		kExtAllocated = 0,
		kExtUserSupplied = 1,
		kExtUserOwned = 2,
	};

	struct shared_info_t {
		shared_info_t();
		shared_info_t(free_function_t fn, void* arg);

		// A pointer to a function to call to free the buffer when the refcount
		// hits 0.  If this is NULL, free() will be used instead.
		free_function_t free_fn;
		void* user_data;
		std::atomic<uint32_t> refcount;
	};

	struct ExternalBuf {
		uint32_t capacity;
		uint32_t type;
		uint8_t* buf;
		// shared_info_t may be NULL if kFlagUserOwned is set.  It is non-NULL
		// in all other cases.
		shared_info_t* sharedInfo;
	};

	struct InternalBuf {
		uint8_t buf[] __attribute__((aligned));
	};

	// The maximum size for an io_buff_t object, including any internal data buffer
	static const uint32_t kMaxIOBufSize = 256;
	static const uint32_t kMaxInternalDataSize;

	// Forbidden copy constructor and assignment opererator
	io_buff_t(io_buff_t const &);
	io_buff_t& operator=(io_buff_t const &);

	/**
	 * Create a new io_buff_t with internal data.
	 *
	 * end is a pointer to the end of the io_buff_t's internal data buffer.
	 */
	explicit io_buff_t(uint8_t* end);

	/**
	 * Create a new io_buff_t pointing to an external buffer.
	 *
	 * The caller is responsible for holding a reference count for this new
	 * io_buff_t.  The io_buff_t constructor does not automatically increment the
	 * reference count.
	 */
	io_buff_t(ExtBufTypeEnum type, uint32_t flags, uint8_t* buf, uint32_t capacity, uint8_t* data, uint32_t length, shared_info_t* shared_info);

	void unshare_one_slow();
	void unshare_chained();
	void coalesce_slow(size_t max_length=std::numeric_limits<size_t>::max());
	// newLength must be the entire length of the buffers between this and
	// end (no truncation)
	void coalesce_and_reallocate(size_t new_headroom, size_t new_length, io_buff_t* end, size_t new_tailroom);
	void decrement_refcount();
	void reserve_slow(uint32_t min_headroom, uint32_t min_tailroom);

	static size_t good_ext_buffer_size(uint32_t min_capacity);
	static void init_ext_buffer(uint8_t* buf, size_t malloc_size, shared_info_t** info_return, uint32_t* capacity_return);
	static void alloc_ext_buffer(uint32_t min_capacity, uint8_t** buf_return, shared_info_t** info_return, uint32_t* capacity_return);

	/*
	 * Member variables
	 */

	/*
	 * Links to the next and the previous io_buff_t in this chain.
	 *
	 * The chain is circularly linked (the last element in the chain points back
	 * at the head), and next_ and prev_ can never be NULL.  If this io_buff_t is the
	 * only element in the chain, next_ and prev_ will both point to this.
	 */
	io_buff_t* next_;
	io_buff_t* prev_;

	/*
	 * A pointer to the start of the data referenced by this io_buff_t, and the
	 * length of the data.
	 *
	 * This may refer to any subsection of the actual buffer capacity.
	*/
	uint8_t* data_;
	uint32_t length_;
	mutable uint32_t flags_;

	union {
		ExternalBuf ext_;
		InternalBuf int_;
	};

	struct deleter_base_t {
		virtual ~deleter_base_t() { }
		virtual void dispose(void* p) = 0;
	};

	template <class unique_ptr_t>
	struct unique_ptr_deleter_t : public deleter_base_t {
		typedef typename unique_ptr_t::pointer pointer_t;
		typedef typename unique_ptr_t::deleter_type deleter_t;

		explicit unique_ptr_deleter_t(deleter_t deleter) : deleter_(std::move(deleter)) {}

		void dispose(void* p) {
			try {
				deleter_(static_cast<pointer_t>(p));
				delete this;
			} catch (...) {
				abort();
			}
		}

	private:
		deleter_t deleter_;
	};

	static void free_unique_ptr_buffer(void* ptr, void* user_data) {
		static_cast<deleter_base_t*>(user_data)->dispose(ptr);
	}
};

template <class unique_ptr_t>
std::unique_ptr<io_buff_t> io_buff_t::take_ownership(unique_ptr_t&& buf, size_t count) {
	size_t size = count * sizeof(typename unique_ptr_t::element_type);
	auto deleter = new unique_ptr_deleter_t<unique_ptr_t>(buf.get_deleter());
	return take_ownership(buf.release(), size, &io_buff_t::free_unique_ptr_buffer, deleter);
}

inline std::unique_ptr<io_buff_t> io_buff_t::copy_buffer(const void* data, uint32_t size, uint32_t headroom, uint32_t min_tailroom) {
	uint32_t capacity = headroom + size + min_tailroom;
	std::unique_ptr<io_buff_t> buf = create(capacity);
	buf->advance(headroom);
	memcpy(buf->writable_data(), data, size);
	buf->append(size);
	return buf;
}

inline std::unique_ptr<io_buff_t> io_buff_t::copy_buffer(const std::string& buf, uint32_t headroom, uint32_t min_tailroom) {
	return copy_buffer(buf.data(), buf.size(), headroom, min_tailroom);
}

inline std::unique_ptr<io_buff_t> io_buff_t::maybe_copy_buffer(const std::string& buf, uint32_t headroom, uint32_t min_tailroom) {
	if (buf.empty()) {
		return nullptr;
	}
	return copy_buffer(buf.data(), buf.size(), headroom, min_tailroom);
}

} // namespace raptor
