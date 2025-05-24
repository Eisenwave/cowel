#include <gtest/gtest.h>

#include "document_file_testing.hpp"

namespace cowel {
namespace {

TEST(Valid, empty)
{
    EXPECT_TRUE(test_for_success(u8"empty.cow"));
}

TEST(Valid, hello_code)
{
    EXPECT_TRUE(test_for_success(u8"hello_code.cow"));
}

TEST(Valid, hello_directive)
{
    EXPECT_TRUE(test_for_success(u8"hello_directive.cow"));
}

TEST(Valid, hello_world)
{
    EXPECT_TRUE(test_for_success(u8"hello_world.cow"));
}

} // namespace
} // namespace cowel
