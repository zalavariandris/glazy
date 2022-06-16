#include "pch.h"

/// test-split digits
// hello_0654 => hello_, 0654
// 23456=>{"", 23456}
// hello_456.jpg => hello_456.jpg

#include "stringutils.h"
#include "pathutils.h"
#include <filesystem>

namespace fs = std::filesystem;

TEST(StringUtilsTexts, test_split_digits)
{
	EXPECT_EQ(split_digits("hello_0654"), std::tuple("hello_", "0654"));
	EXPECT_EQ(split_digits("12345"), std::tuple("", "12345"));
	EXPECT_EQ(split_digits("hello_12345.jpg"), std::tuple("hello_12345.jpg", ""));
}

TEST(SequenceFromItem, test_find_sequence)
{
	EXPECT_EQ(
		sequence_from_item("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.0004.exr"),
		std::tuple("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr", 1, 8, 4)
	);
}

TEST(SequenceFromItem, test_bad_item)
{
	EXPECT_THROW({
		try {
			sequence_from_item("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/NO_FILE.0004.exr");
		}
		catch (const std::filesystem::filesystem_error& e) {
			EXPECT_STREQ("no such file or directory", e.code().message().c_str());
			throw;
		}
		}, std::filesystem::filesystem_error);
}

TEST(SequenceFromItem, test_bad_folder)
{
	EXPECT_THROW({
		try {
			sequence_from_item("C:/Users/andris/Desktop/testimages/openexr-images-master/NO_FOLDER/singlepart.0004.exr");
		}
		catch (const std::filesystem::filesystem_error& e) {
			EXPECT_STREQ("no such file or directory", e.code().message().c_str());
			throw;
		}
		}, std::filesystem::filesystem_error);
}

TEST(SequenceFromPattern, test_find_sequence)
{
	EXPECT_EQ(
		sequence_from_pattern("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr"),
		std::tuple("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.%04d.exr", 1, 8)
	);

}

TEST(SequenceFromPattern, test_bad_filename)
{
	EXPECT_THROW({
	try {
			sequence_from_pattern("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/no_file.%04d.exr");
		}
		catch (const std::filesystem::filesystem_error& e) {
			EXPECT_STREQ("no such file or directory", e.code().message().c_str());
			throw;
		}
	}, std::filesystem::filesystem_error);
}

TEST(SequenceFromPattern, test_bad_folder)
{
	EXPECT_THROW({
	try {
			sequence_from_pattern("C:/Users/andris/Desktop/testimages/openexr-images-master/NO_FOLDER/singlepart.%04d.exr");
		}
		catch (const std::filesystem::filesystem_error& e) {
			EXPECT_STREQ("no such file or directory", e.code().message().c_str());
			throw;
		}
		}, std::filesystem::filesystem_error);
}

TEST(SequenceFromPattern, test_mispelled_patternformat)
{
	EXPECT_THROW({
		try {
				sequence_from_pattern("C:/Users/andris/Desktop/testimages/openexr-images-master/Beachball/singlepart.04.exr");
			}
		catch (const std::filesystem::filesystem_error& e) {
			EXPECT_STREQ("no such file or directory", e.code().message().c_str());
			throw;
		}
	}, std::filesystem::filesystem_error);
}