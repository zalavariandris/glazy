
#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace ImageIO {

	struct Spec {
		int width=0;
		int height=0;
		int nchannels=0;
	};

	/* get layers by convention */
	std::vector<std::string> get_layers(const std::filesystem::path path);

	/* get chanels by convention */
	std::vector<std::string> get_channels(std::filesystem::path path, std::string layer);

	/* get pixels */
	void get_pixels(std::filesystem::path path, std::string layer, float* data);
}