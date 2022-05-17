#pragma once
#include "../FileSequence.h"
#include "BaseSequenceReader.h"

class OIIOSequenceReader : public BaseSequenceReader
{
public:
	OIIOSequenceReader(const FileSequence& seq);
	void onGUI() override;

	// attributes
	std::tuple<int, int> size() override; /// display size
	std::tuple<int, int, int, int> bbox() override; /// data bounding box

	int current_frame() override { return m_current_frame; }
	void set_current_frame(int f) override { m_current_frame = f; }

	int selected_part_idx() override { return m_selected_part_idx; }
	void set_selected_part_idx(int p) override { m_selected_part_idx = p; }

	std::vector<std::string> selected_channels() override;
	void set_selected_channels(std::vector<std::string> channels) override;

	// calculate
	void read_to_memory(void* memory) override;

private:
	FileSequence m_sequence;
	std::tuple<int, int, int, int> m_bbox; // data window
	int display_x, display_y, display_width, display_height; // display window
	int m_current_frame; // current selected frame
	int m_selected_part_idx = 0;
	std::vector<std::string> mSelectedChannels{ "R", "G", "B", "A"}; // channels to actually read
};

