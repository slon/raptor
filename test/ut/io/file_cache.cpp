#include <raptor/io/file_cache.h>

#include <stdexcept>
#include <system_error>
#include <thread>

#include <gmock/gmock.h>

#include "mock_posix_api.h"

using namespace raptor;
using namespace ::testing;

TEST(file_cache_test_t, DISABLED_open) {
	mock_posix_api_t posix;
	EXPECT_CALL(posix, open("/etc/passwd", O_RDONLY))
		.WillOnce(Return(42));

	file_cache_t cache(1);

	std::shared_ptr<const file_cache_t::file_t> file = cache.open("/etc/passwd");

	ASSERT_EQ(42, file->fd);
}

TEST(file_cache_test_t, DISABLED_reopens) {
	mock_posix_api_t posix;
	EXPECT_CALL(posix, open("/file", O_RDONLY))
		.WillOnce(Return(42))
		.WillOnce(Return(20));

	EXPECT_CALL(posix, close(42))
		.WillOnce(Return(0));

	file_cache_t cache(1, std::chrono::milliseconds(50));
	auto file = cache.open("/file");
	ASSERT_EQ(42, file->fd);
	file = cache.open("/file");
	ASSERT_EQ(42, file->fd);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	file = cache.open("/file");
	ASSERT_EQ(20, file->fd);
}

TEST(file_cache_test_t, DISABLED_many_files) {
	mock_posix_api_t posix;
	EXPECT_CALL(posix, open("/file1", O_RDONLY)).WillOnce(Return(1));
	EXPECT_CALL(posix, open("/file2", O_RDONLY)).WillOnce(Return(2));
	EXPECT_CALL(posix, open("/file3", O_RDONLY)).WillOnce(Return(3));

	file_cache_t cache(3);
	ASSERT_EQ(1, cache.open("/file1")->fd);
	ASSERT_EQ(2, cache.open("/file2")->fd);
	ASSERT_EQ(3, cache.open("/file3")->fd);

	ASSERT_EQ(3, cache.open("/file3")->fd);
	ASSERT_EQ(2, cache.open("/file2")->fd);
	ASSERT_EQ(1, cache.open("/file1")->fd);
}

TEST(file_cache_test_t, DISABLED_fds_evicted) {
	mock_posix_api_t posix;
	EXPECT_CALL(posix, open("/f2", O_RDONLY)).WillOnce(Return(2));

	file_cache_t cache(1);
	ASSERT_EQ(2, cache.open("/f2")->fd);

	EXPECT_CALL(posix, open("/f3", O_RDONLY)).WillOnce(Return(3));
	EXPECT_CALL(posix, close(2)).WillOnce(Return(0));

	ASSERT_EQ(3, cache.open("/f3")->fd);
}

TEST(file_cache_test_t, DISABLED_destructor_closes_fds) {
	mock_posix_api_t posix;
	EXPECT_CALL(posix, open("/f", _)).WillOnce(Return(20));

	auto cache = std::make_shared<file_cache_t>(1);
	cache->open("/f");

	EXPECT_CALL(posix, close(20)).WillOnce(Return(0));
	cache.reset();
}

TEST(file_cache_test_t, DISABLED_exception) {
	mock_posix_api_t posix;
	EXPECT_CALL(posix, open("/not_found", _)).WillOnce(SetErrnoAndReturn(ENOENT, -1));

	file_cache_t cache(1);
	ASSERT_THROW(cache.open("/not_found"), std::system_error);
}
