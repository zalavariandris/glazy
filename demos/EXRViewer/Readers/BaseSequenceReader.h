#pragma once
#include <tuple>
#include <vector>
#include <string>

class BaseSequenceReader
{
public:
	virtual void onGUI(){}

	virtual std::tuple<int, int> size() = 0;
	virtual std::tuple<int, int, int, int> bbox() = 0;

	virtual int current_frame() = 0;
	virtual void set_current_frame(int f) = 0;

	virtual int selected_part_idx() = 0;
	virtual void set_selected_part_idx(int p) = 0;

	virtual std::vector<std::string> selected_channels()=0;
	virtual void set_selected_channels(std::vector<std::string> channels)=0;

	// calculate
	virtual void read()=0;

	void* memory=NULL;
};