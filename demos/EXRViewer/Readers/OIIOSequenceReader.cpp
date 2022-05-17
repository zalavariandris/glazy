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

std::vector<std::string> OIIOSequenceReader::selected_channels()
{

	return mSelectedChannels;
}
void OIIOSequenceReader::set_selected_channels(std::vector<std::string> channels)
{
	mSelectedChannels = channels;
}

void OIIOSequenceReader::read_to_memory(void* memory)
{
	if (mSelectedChannels.empty()) return;

	//
	auto filename = m_sequence.item(m_current_frame);
	auto file = OIIO::ImageInput::open(filename.string());
	
	int miplevel = 0;
	file->seek_subimage(m_selected_part_idx, 0);
	
	auto spec = file->spec();
	auto channels_count = mSelectedChannels.size();
	char* ptr = (char*)memory;
	auto typesize = OIIO::TypeHalf.size();

	for (auto i = 0;i< channels_count;i++)
	{
		const std::string& channel_name = mSelectedChannels.at(i);

		auto it = find(spec.channelnames.begin(), spec.channelnames.end(), channel_name);
		if (it != spec.channelnames.end())
		{
			int chbegin = it - spec.channelnames.begin();

			file->read_image(
				m_selected_part_idx, 
				miplevel, 
				chbegin, 
				chbegin+1, 
				OIIO::TypeDesc::HALF, // type
				&ptr[i* typesize], //data
				typesize*channels_count, // xstride
				typesize*channels_count*spec.width, //ystride
				OIIO::AutoStride // zstride
			);
		}
	}

	// update atributes
	auto fmt = spec.format;
	bool is_half = fmt == OIIO::TypeDesc::HALF;
	m_bbox = std::tuple<int, int, int, int>(spec.x, spec.y, spec.width, spec.height);

	file->close();
}