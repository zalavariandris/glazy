#include "pch.h"

#include "glazy.h"
#include "OOGL/SmartGLObject.h"
#include "OOGL/Program.h"

TEST(TestCaseName, TestName) {

	glazy::init();
	glazy::new_frame();

	auto program = OOGL::Program();
	std::cout << "has gl object: " << program.HasGLObject() << std::endl;
	std::cout << "id: " << program.id() << std::endl;
	std::cout << "ref count: " << program.ref_count() << std::endl;


	glazy::end_frame();
	glazy::destroy();

	EXPECT_EQ(1, 1);
	EXPECT_TRUE(true);
}

