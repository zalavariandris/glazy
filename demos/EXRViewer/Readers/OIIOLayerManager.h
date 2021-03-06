#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include "OpenImageIO/imageio.h"

#include "BaseLayerManager.h"
#include <functional>

class OIIOLayerManager : LayerManager
{
public:
	OIIOLayerManager(const std::filesystem::path& filename);
	bool onGUI() override;

	const std::vector<Layer> layers() const override
	{
		return mLayers;
	}
	
	void set_selected_layer(int selection) override
	{
		// std::find(mLayers.begin(), mLayers.end(), *selection); TODO: Make sure the layer exists here!
		mSelectedLayer = &mLayers.at(selection);
	}
	
	const Layer* selected_layer() const override {
		return mSelectedLayer;
	};

	void on_change(std::function<void(Layer*)> handle) {
		handlers.push_back(handle);
	}


private:
	std::vector<Layer> mLayers;
	const Layer* mSelectedLayer=NULL;
	std::vector<std::function<void(Layer*)>> handlers;
};

