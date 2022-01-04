#include "pch.h"

#include "imageio.h"
#include <filesystem>


std::filesystem::path openexr_images = "C:/Users/andris/Desktop/test_images/openexr-images-master";

TEST(IMAGEIO_TEST_CASE, single_rgb) {
	auto file = openexr_images / "Beachball" / "singlepart.0001.exr";
	auto layers = ImageIO::get_layers(file);
	for (auto layer : layers) std::cout << "- " << layer << "\n";

	EXPECT_EQ(layers, std::vector<std::string>({ "color", "depth", "disparityL", "disparityR", "forward.left", "forward.right", "left", "whitebarmask.left", "whitebarmask.right" }));

	auto color_channels = ImageIO::get_channels(file, "color");
	//for (auto channel : color_channels) std::cout << "- " << channel << "\n";
	EXPECT_EQ(color_channels, std::vector<std::string>({ "R", "G", "B", "A"}));

	//auto depth_channels = get_channels_by_convention(file, "depth");
	//EXPECT_EQ(depth_channels, std::vector<std::string>({ "" }));

	auto disparity_channels = ImageIO::get_channels(file, "disparityL");
	EXPECT_EQ(disparity_channels, std::vector<std::string>({ "x", "y" }));

	auto whitebarmaks_channels = ImageIO::get_channels(file, "whitebarmask.left");
	EXPECT_EQ(whitebarmaks_channels, std::vector<std::string>({ "mask" }));
}

/*
TEST(IMAGEIO_TEST_CASE, multipart_subimages_test) {
	auto file = openexr_images / "v2/LeftView" / "Tree.exr";
	auto subimages = get_subimages(file);
	std::cout << "layers: " << "\n";
	for (auto subimage : subimages) std::cout << "- " << subimage << "\n";
	EXPECT_EQ(1, 1);
	EXPECT_TRUE(true);
}
*/

/*
TEST(IMAGEIO_TEST_CASE, Singlepart_layers_test) {
	auto file = openexr_images / "Beachball" / "singlepart.0001.exr";
	auto layers = get_layers(file);
	std::cout << "layers: " << "\n";
	for (auto layer : layers) std::cout << "- " << layer << "\n";
	EXPECT_EQ(1, 1);
	EXPECT_TRUE(true);
}
*/

