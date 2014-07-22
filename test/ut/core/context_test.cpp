#include <raptor/core/context.h>

#include <stdexcept>
#include <thread>

#include <gmock/gmock.h>

using namespace raptor;
using namespace raptor::internal;
using namespace ::testing;

struct test_contexts_t {
	context_t main, t1, t2, t3;
};

std::vector<char> T1_STACK(1024 * 1024);
std::vector<char> T2_STACK(1024 * 1024);
std::vector<char> T3_STACK(1024 * 1024);

static void switch_t1_to_main(void* p) {
	test_contexts_t* ctx = (test_contexts_t*)p;

	ctx->t1.switch_to(&ctx->main);
}

TEST(context_test_t, simple_switch) {
	test_contexts_t ctx;

	ctx.t1.create(T1_STACK.data(), T1_STACK.size(), switch_t1_to_main, &ctx);
	ctx.main.switch_to(&ctx.t1);

	SUCCEED();
}

static void throw_exception(void* ) {
	throw std::runtime_error("take that!");
}

TEST(context_death_test_t, uncaught_exception_asserts) {
	test_contexts_t ctx;
	ctx.t1.create(T1_STACK.data(), T1_STACK.size(), throw_exception, &ctx);

	ASSERT_DEATH(ctx.main.switch_to(&ctx.t1), ".*terminate called after throwing an instance of 'std::runtime_error'.*");
}

static void do_nothing(void*) {}

TEST(context_death_test_t, return_context_function_asserts) {
	test_contexts_t ctx;
	ctx.t1.create(T1_STACK.data(), T1_STACK.size(), do_nothing, &ctx);

	ASSERT_DEATH(ctx.main.switch_to(&ctx.t1), ".*return from func\\(arg\\).*");
}

static bool ERRNO_PTR_CHANGED, PTHREAD_SELF_CHANGED;

static void check_errno_and_pthread_self(void* p) {
	test_contexts_t* ctx = (test_contexts_t*)p;

	int* e = &errno;
	pthread_t s = pthread_self();

	ctx->t1.switch_to(&ctx->main);

	ERRNO_PTR_CHANGED = (e != &errno);
	PTHREAD_SELF_CHANGED = (s != pthread_self());

	ctx->t1.switch_to(&ctx->main);
}

TEST(context_test_t, errno_and_pthread_self_not_optimized_away) {
	ERRNO_PTR_CHANGED = true;
	PTHREAD_SELF_CHANGED = true;

	test_contexts_t ctx;
	ctx.t1.create(T1_STACK.data(), T1_STACK.size(), check_errno_and_pthread_self, &ctx);
	
	ctx.main.switch_to(&ctx.t1);

	std::thread([&ctx] () {
		ctx.main.switch_to(&ctx.t1);
	}).join();

	ASSERT_TRUE(ERRNO_PTR_CHANGED);
	ASSERT_TRUE(PTHREAD_SELF_CHANGED);
}

int SEQ_NUMBER;

struct test_exception_t {};

struct switch_context_t {
	test_contexts_t* ctx;

	~switch_context_t() {
		assert(SEQ_NUMBER == 0);
		assert(std::uncaught_exception());
		SEQ_NUMBER = 1;

		ctx->t1.switch_to(&ctx->main);

		assert(SEQ_NUMBER == 2);
		assert(std::uncaught_exception());
		SEQ_NUMBER = 3;
	}
};

static void throw_exception_and_switch(void* p) {
	test_contexts_t* ctx = (test_contexts_t*)p;

	try {
		switch_context_t sc;
		sc.ctx = ctx;

		throw test_exception_t();
	} catch (const test_exception_t& exception) {
		assert(!std::uncaught_exception());
		assert(std::current_exception() != nullptr);
		assert(SEQ_NUMBER == 3);
		SEQ_NUMBER = 4;

		ctx->t1.switch_to(&ctx->main);
	}

	ctx->t1.switch_to(&ctx->main);
}

TEST(context_test_t, switch_while_exception_in_flight) {
	SEQ_NUMBER = 0;

	test_contexts_t ctx;
	ctx.t1.create(T1_STACK.data(), T1_STACK.size(), throw_exception_and_switch, &ctx);

	ctx.main.switch_to(&ctx.t1);
	ASSERT_FALSE(std::uncaught_exception());
	ASSERT_TRUE(!std::current_exception());
	if(SEQ_NUMBER == 1) SEQ_NUMBER = 2;

	ctx.main.switch_to(&ctx.t1);
	ASSERT_FALSE(std::uncaught_exception());
	ASSERT_TRUE(!std::current_exception());
	if(SEQ_NUMBER == 4) SEQ_NUMBER = 5;

	ctx.main.switch_to(&ctx.t1);
	ASSERT_FALSE(std::uncaught_exception());
	ASSERT_TRUE(!std::current_exception());
	ASSERT_EQ(5, SEQ_NUMBER);
}

static int CIRCLE_NUM;

static void switch_t1_to_t2(void* p) {
	test_contexts_t* ctx = (test_contexts_t*)p;
	if(CIRCLE_NUM == 0) CIRCLE_NUM = 1;

	ctx->t1.switch_to(&ctx->t2);
}

static void switch_t2_to_t3(void* p) {
	test_contexts_t* ctx = (test_contexts_t*)p;
	if(CIRCLE_NUM == 1) CIRCLE_NUM = 2;

	ctx->t2.switch_to(&ctx->t3);
}

static void switch_t3_to_main(void* p) {
	test_contexts_t* ctx = (test_contexts_t*)p;
	if(CIRCLE_NUM == 2) CIRCLE_NUM = 3;

	ctx->t3.switch_to(&ctx->main);
}

TEST(context_test_t, circle_switch) {
	test_contexts_t ctx;
	ctx.t1.create(T1_STACK.data(), T1_STACK.size(), switch_t1_to_t2, &ctx);
	ctx.t2.create(T2_STACK.data(), T2_STACK.size(), switch_t2_to_t3, &ctx);
	ctx.t3.create(T3_STACK.data(), T3_STACK.size(), switch_t3_to_main, &ctx);

	CIRCLE_NUM = 0;
	ctx.main.switch_to(&ctx.t1);
	ASSERT_EQ(3, CIRCLE_NUM);
}
