#include <raptor/kafka/blob.h>

#include <gtest/gtest.h>

using namespace raptor::kafka;

TEST(blob_t, full_test) {
	std::shared_ptr<const char> buffer(
		new char[20],
		[] (const char* ptr) { delete[] ptr; }
	);

	blob_t full(buffer, 20);

	ASSERT_EQ(20U, full.size());
	ASSERT_EQ(buffer.get(), full.data());

	blob_t slice = full.slice(5, 10);
	ASSERT_EQ(10U, slice.size());
	ASSERT_EQ(buffer.get() + 5, slice.data());

	blob_t copy = full.slice(0, 20);
	ASSERT_EQ(20U, copy.size());
	ASSERT_EQ(buffer.get(), copy.data());

	blob_t slice_copy = slice.slice(0, 10);
	ASSERT_EQ(10U, slice_copy.size());
	ASSERT_EQ(buffer.get() + 5, slice_copy.data());
}
