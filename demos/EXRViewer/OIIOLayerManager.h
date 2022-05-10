#pragma once
#include <vector>
#include <string>

class OIIOLayerManager
{
	std::vector<std::string> layer_names();
	std::string current_name();
	int current();
	void set_current(int index);

	int current_part_idx();
	std::vector<std::string> current_channel_ids();

	bool onGUI();
};

