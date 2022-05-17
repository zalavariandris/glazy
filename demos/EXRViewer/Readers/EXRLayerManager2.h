#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <functional>
#include "OpenImageIO/imageio.h"

#include "BaseLayerManager.h"

class EXRLayerManager2 : LayerManager
{
public:
	EXRLayerManager2(const std::filesystem::path& filename);
	bool onGUI() override;

	const std::vector<Layer> layers() const override
	{
		return mLayers;
	}

	void set_selected_layer(int selection) override
	{
		// std::find(mLayers.begin(), mLayers.end(), *selection); TODO: Make sure the layer exists here!
		for (auto handle : handlers) {
			handle(&mLayers.at(selection));
		}
		mSelectedLayer = &mLayers.at(selection);
	}

	const Layer* selected_layer() const override { return mSelectedLayer; };

	void on_change(std::function<void(Layer*)> handle) {
		handlers.push_back(handle);
	}

private:
	std::vector<Layer> mLayers;
	const Layer* mSelectedLayer = NULL;
	std::vector<std::function<void(Layer*)>> handlers;
};