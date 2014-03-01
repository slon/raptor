#include <raptor/core/context.h>

#include <string.h>

#include <cassert>
#include <system_error>
#include <stdexcept>

namespace raptor { namespace internal {

context_t::context_t() {
	SP_ = nullptr;
	memset(&EH_, 0, sizeof(EH_));

	func_ = nullptr;
	arg_ = nullptr;
}

void context_t::second_trampoline(void* ctx) {
	context_t* context = (context_t*)ctx;

	void (*func)(void*) = context->func_;
	void* arg = context->arg_;

	context->func_ = nullptr;
	context->arg_ = nullptr;

	func(arg);

	assert(!"return from func(arg)");
}

void context_t::create(char* stack, int size, void (*func)(void*), void* arg) {
	SP_ = reinterpret_cast<void**>(reinterpret_cast<char*>(stack) + size);
	func_ = func;
	arg_ = arg;

	// We pad an extra nullptr to align %rsp before callq in second
	// trampoline.  Effectively, this nullptr mimics a return address.
	*--SP_ = nullptr;
	*--SP_ = (void*) &context_t::trampoline;
	// See |context-supp.s| for precise register mapping.
	*--SP_ = nullptr;                               // %rbp
	*--SP_ = (void*) &context_t::second_trampoline; // %rbx
	*--SP_ = (void*) this;                          // %r12
	*--SP_ = nullptr;                               // %r13
	*--SP_ = nullptr;                               // %r14
	*--SP_ = nullptr;                               // %r15
}

void context_t::switch_to(context_t* to) {
	__cxxabiv1::__cxa_eh_globals* currentEH = __cxxabiv1::__cxa_get_globals();
	EH_ = *currentEH; *currentEH = to->EH_;

	swap(this, to);
}

}} // namespace raptor::internal
