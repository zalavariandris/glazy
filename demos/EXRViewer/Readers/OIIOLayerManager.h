#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include "OpenImageIO/imageio.h"



class OIIOLayerManager
{
public:
	struct Channel
	{
		int idx;
		int subimage;
		std::string name; // full name
	};

	struct Layer {
		std::string name;
		int part;
		std::vector<Channel> channels;
		std::vector<Layer> children;

		Layer(std::string name, int part, std::vector<Channel> channels):
			name(name),
			part(part),
			channels(channels)
		{}
	};

	OIIOLayerManager(const std::filesystem::path& filename);
	bool onGUI();

	void set_layer(int idx) {
		current_layer_idx = idx;
	}

	//std::vector<std::string> layer_names();
	//std::string current_name();
	//int current();
	//void set_current(int index);

	//int current_part_idx();
	//std::vector<std::string> current_channel_ids();
private:
	void show_layer_in_table(const OIIOLayerManager::Layer& layer);
	std::filesystem::path mFilename;
	std::unique_ptr<OIIO::ImageInput> file;
	std::vector<Layer> mLayers;
	std::vector<std::tuple<int, std::string>> mSelectedChannels;
	int current_layer_idx=0;
};

