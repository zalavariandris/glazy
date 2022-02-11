#include "pch.h"

/// test-split digits
// hello_0654 => hello_, 0654
// 23456=>{"", 23456}
// hello_456.jpg => hello_456.jpg

#include "stringutils.h"

TEST(StringUtilsTexts, test_split_digits)
{
	EXPECT_EQ(split_digits("hello_0654"), std::tuple("hello_", "0654"));
	EXPECT_EQ(split_digits("12345"), std::tuple("", "12345"));
	EXPECT_EQ(split_digits("hello_12345.jpg"), std::tuple("hello_12345.jpg", ""));
}