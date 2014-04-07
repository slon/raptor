#pragma once

namespace __cxxabiv1 {
    // We do not care about actual type here, so erase it.
    typedef void __untyped_cxa_exception;
    struct __cxa_eh_globals {
        __untyped_cxa_exception* caughtExceptions;
        unsigned int uncaughtExceptions;
    };
    extern "C" __cxa_eh_globals* __cxa_get_globals() throw();
    extern "C" __cxa_eh_globals* __cxa_get_globals_fast() throw();
} // namespace __cxxabiv1

namespace raptor { namespace internal {

class context_t {
public:
	context_t();

	void create(char* stack, int size, void (*func)(void*), void* arg);
	void switch_to(context_t* to);

	context_t(const context_t&) = delete;
	context_t(context_t&&) = delete;

	context_t& operator = (const context_t&) = delete;
	context_t& operator = (context_t&&) = delete;

private:
	void** SP_;
	__cxxabiv1::__cxa_eh_globals EH_;

	void (*func_)(void*);
	void *arg_;

	static void swap(context_t* from, context_t* to);

	static void trampoline();

	// second trampoline asserts that func(arg) never returns
	static void second_trampoline(void* ctx);
};

}} // namespace raptor::internal
