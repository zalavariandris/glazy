#pragma once
#include "FileSequence.h"

class EXRSequenceReader {
private:
    FileSequence sequence;

    // the actial pixels data and display dimensions 
    std::tuple<int, int, int, int> m_bbox; // data window
    int display_x, display_y, display_width, display_height; // display window
    std::vector<std::string> mSelectedChannels{ "A", "B", "G", "R" }; // channels to actually read

public:
    int current_frame; // current selected frame
    int selected_part_idx = 0;
    

    EXRSequenceReader(const FileSequence& seq);
    void onGUI();

    // attributes
    std::tuple<int, int> size(); /// display size
    std::tuple<int, int, int, int> bbox(); /// data bounding box
    std::vector<std::string> selected_channels(); ///
    void set_selected_channels(std::vector<std::string> channels);
 
    // calculate
    void read_to_memory(void* memory);
};

