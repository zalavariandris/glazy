#pragma once
#include <vector>
#include <string>
#include <filesystem>

struct Layer {
	std::string name;
	int part;
	std::vector<std::string> channels;
};

class LayerManager
{
public:
	virtual const std::vector<Layer> layers() const = 0;
	virtual void set_selected_layer(int selection) = 0;
	virtual const Layer* selected_layer() const = 0;
	virtual bool onGUI() { return false; }

protected:
	LayerManager() {};
};

