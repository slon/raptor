#include <raptor/io/io_buff.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(io_buff_t, compiles) {}

TEST(io_buff_t, create) {
	for(size_t size : { 0, 1, 10, 100, 10000 }) {
		auto buff1 = io_buff_t::create(size);
		ASSERT_TRUE(buff1->empty());
		ASSERT_FALSE(buff1->is_chained());
		ASSERT_LE(size, buff1->buffer_end() - buff1->buffer());
	}
}

TEST(io_buff_t, create_chain) {
	for(size_t size : { 0, 1, 10, 100, 1000, 10000 }) {
		auto buff1 = io_buff_t::create_chain(size, 1000);
		ASSERT_TRUE(buff1->empty());

		size_t total = 0;
		const io_buff_t* b = buff1.get();
		do {
			size_t b_size = b->buffer_end() - b->buffer();
			ASSERT_GE(1000, b_size);
			total += b_size;

			b = b->next();
		} while(b != buff1.get());

		ASSERT_LE(size, total);
	}
}

TEST(io_buff_t, wrap_buffer) {
	for(size_t size : { 0, 1, 10, 100, 1000, 10000 }) {
		uint8_t buffer[size];
		auto buff1 = io_buff_t::wrap_buffer(buffer, sizeof(buffer));

		ASSERT_EQ(&buffer[0], buff1->buffer());
		ASSERT_EQ(&buffer[0], buff1->data());
		ASSERT_EQ(&buffer[size], buff1->buffer_end());
		ASSERT_EQ(&buffer[size], buff1->tail());
	}
}


void assert_shape(const std::unique_ptr<io_buff_t>& b, size_t headroom, size_t data_size, size_t tailroom) {
	if(b->empty()) {
		ASSERT_EQ(0, b->length());
	} else {
		ASSERT_NE(0, b->length());
	}

	ASSERT_EQ(headroom, b->headroom());
	ASSERT_EQ(headroom, b->data() - b->buffer());

	ASSERT_EQ(data_size, b->length());
	ASSERT_EQ(data_size, b->tail() - b->data());

	ASSERT_EQ(tailroom, b->tailroom());
	ASSERT_EQ(tailroom, b->buffer_end() - b->tail());

	ASSERT_EQ(headroom + data_size + tailroom, b->capacity());
}

TEST(io_buff_t, data_ops) {
	auto b = io_buff_t::create(1000);
	assert_shape(b, 0, 0, 1000);

	b->append(50);
	assert_shape(b, 0, 50, 950);

	b->trim_start(10);
	assert_shape(b, 10, 40, 950);

	b->trim_end(10);
	assert_shape(b, 10, 30, 960);

	b->prepend(5);
	assert_shape(b, 5, 35, 960);

	b->clear();
	assert_shape(b, 0, 0, 1000);
}
