#include "pch.h"

#include "glazy.h"
#include "OOGL/SmartGLObject.h"

TEST(TestCaseName, TestName) {

	glazy::init();
	glazy::new_frame();


	glazy::end_frame();

  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}

