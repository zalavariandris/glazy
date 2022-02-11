#include "pch.h"
#include "splitexr.h"

void test_collect_files() {

}

TEST(TestCaseName, TestName) {
    auto results = collect_input_files({ "C:/Users/andris/Downloads/52_06/52_06_EXAM_v06-vrayraw.0003.exr", "World" });

    auto expected = std::vector<std::filesystem::path>{
        "C:/Users/andris/Downloads/52_06/52_06_EXAM_v06-vrayraw.0002.exr",
        "C:/Users/andris/Downloads/52_06/52_06_EXAM_v06-vrayraw.0003.exr"
    };

    EXPECT_EQ(results, expected);

    //std::cout << "test_collect_files: " << (results == expected ? "passed" : "failed") << "\n";


  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}