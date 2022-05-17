#pragma once
#include "../FileSequence.h"


class OIIOSequenceReader
{
public:
	struct Channel {
		int part;
		std::string channel;
	};
	OIIOSequenceReader(const FileSequence& seq);
	void onGUI();

	// attributes
	std::tuple<int, int> size();
	std::tuple<int, int, int, int> bbox();

	int current_frame() { return m_current_frame; }
	void set_current_frame(int f) { m_current_frame = f; }

	int selected_part_idx() { return m_selected_part_idx; }
	void set_selected_part_idx(int p) { m_selected_part_idx = p; }

	std::vector<std::string> selected_channels();
	void set_selected_channels(std::vector<std::string> channels);

	// calculate
	void read_to_memory(void* memory);
private:
	std::tuple<int, int, int, int> m_bbox; // data window
	int display_x, display_y, display_width, display_height; // display window
	FileSequence m_sequence;
	int m_current_frame; // current selected frame
	int m_selected_part_idx = 0;
	std::vector<std::string> mSelectedChannels{ "R", "G", "B", "A"}; // channels to actually read
};

