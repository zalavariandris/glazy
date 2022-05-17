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

	/// <summary>
	/// by selecting a layer, return selected part and channels

	int selected_part()
	{
		if (this->selected_layer == NULL) {
			return -1;
		}

		return this->selected_layer->part;
	}

	std::vector<std::string> selected_channels()
	{
		if (this->selected_layer == NULL) {
			return {};
		}
		const auto& layer = this->selected_layer;
		std::vector<std::string> channel_ids;
		for (const auto& channel : layer->channels) {
			channel_ids.push_back(channel.name);
		}
		return channel_ids;
	}

private:
	std::filesystem::path mFilename;
	std::unique_ptr<OIIO::ImageInput> file;

	std::vector<Layer> mLayers;
	const Layer* selected_layer=NULL;
};

