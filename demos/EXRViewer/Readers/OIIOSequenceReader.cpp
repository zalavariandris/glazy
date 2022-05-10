#include "OIIOSequenceReader.h"
#include "OpenImageIO/imageio.h"
#include "imgui.h"

OIIOSequenceReader::OIIOSequenceReader(const FileSequence& seq)
{
	m_sequence = seq;
	m_current_frame = m_sequence.first_frame;

	// OpenFile
	auto filename = m_sequence.item(m_sequence.first_frame);

	auto first_file = OIIO::ImageInput::open(filename.string());

	auto spec = first_file->spec();
	display_x = spec.full_x;
	display_y = spec.full_y;
	display_width = spec.full_width;
	display_height = spec.full_height;
}

void OIIOSequenceReader::onGUI() {
	ImGui::DragInt("current frame", &m_current_frame);
}

std::tuple<int, int> OIIOSequenceReader::size() {
	return { display_width, display_height };
}

std::tuple<int, int, int, int> OIIOSequenceReader::bbox() {
	return m_bbox;
}

std::vector<std::string> OIIOSequenceReader::selected_channels() {
	return mSelectedChannels;
}
void OIIOSequenceReader::set_selected_channels(std::vector<std::string> channels)
{
	mSelectedChannels = channels;
}

void OIIOSequenceReader::read_to_memory(void* memory) {
	if (mSelectedChannels.empty()) return;

	//
	auto filename = m_sequence.item(m_current_frame);
	auto file = OIIO::ImageInput::open(filename.string());
	
	auto spec = file->spec();
	int subimage = 0;
	int miplevel = 0;
	int chbegin = 0;
	int chend = 4;
	file->read_image(subimage, miplevel, chbegin, chend, OIIO::TypeDesc::HALF, memory);
	// update atributes
	auto fmt = spec.format;
	bool is_half = fmt == OIIO::TypeDesc::HALF;
	m_bbox = std::tuple<int, int, int, int>(spec.x, spec.y, spec.width, spec.height);

	file->close();
}