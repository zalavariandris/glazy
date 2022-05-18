#include "OIIOSequenceReader.h"
#include "OpenImageIO/imageio.h"
#include "imgui.h"
#include "../tracy/Tracy.hpp"

#include "OpenImageIO/imagecache.h"

OIIOSequenceReader::OIIOSequenceReader(const FileSequence& seq)
{
	m_sequence = seq;
	m_current_frame = m_sequence.first_frame;

	// OpenFile
	auto filename = m_sequence.item(m_sequence.first_frame);

	auto file = OIIO::ImageInput::open(filename.string());

	auto spec = file->spec();
	display_x = spec.full_x;
	display_y = spec.full_y;
	display_width = spec.full_width;
	display_height = spec.full_height;

	image_cache = OIIO::ImageCache::create(true);
	image_cache->attribute("max_memory_MB", 16000.0f);
	//image_cache->attribute("autotile", 64);
	//image_cache->attribute("max_open_files", 2);
}

void OIIOSequenceReader::onGUI()
{
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

void OIIOSequenceReader::read()
{
	//ZoneScoped;
	if (mSelectedChannels.empty()) return;
	assert(("memory address is NULL", memory != NULL));

	auto filename = m_sequence.item(m_current_frame);
	
	OIIO::ImageSpec spec;
	
	/// Open current subimage
	
	std::unique_ptr<OIIO::ImageInput> file;
	//OIIO::ImageSpec spec;
	{
		
		auto filename = m_sequence.item(m_current_frame);
		if (!std::filesystem::exists(filename)) {
			std::cerr << "file does not exist: " << filename << "\n";
			return;
		}

		file = OIIO::ImageInput::open(filename.string());

		if (!file) {
			std::cerr << "OIIO cant open this file" << "\n";
			std::cerr << "  " << OIIO::geterror() << "\n";
		}

		file->seek_subimage(m_selected_part_idx, 0);
		spec = file->spec();
	}

	/// Update datawindow
	m_bbox = std::tuple<int, int, int, int>(
		spec.x,
		spec.y,
		spec.width,
		spec.height);

	/// Read pixels
	{
		
		// get channel indices
		std::vector<int> channel_indices;
		{
			auto channelnames = spec.channelnames;
			for (auto channel_name : mSelectedChannels) {
				auto it = std::find(channelnames.begin(), channelnames.end(), channel_name);
				if (it != channelnames.end()) {
					int index = it - channelnames.begin();
					channel_indices.push_back(index);
				}
			}
		}

		// read channels
		{
			int channels_count = channel_indices.size();
			char* ptr = (char*)memory;
			auto typesize = spec.format.size();
			auto width = spec.width;

			const auto [min, max] = std::minmax_element(channel_indices.begin(), channel_indices.end());
			int chbegin = *min;
			int chend = *max+1;
			if (chend - chbegin != channel_indices.size()) {
				throw std::exception("channels are not in order");
			}
			file->read_image(chbegin, chend, OIIO::TypeDesc::HALF, memory);

			/// read each color plate seperatelly
			//for (auto i = 0; i < channels_count; i++)
			//{
			//	int chbegin = channel_indices.at(i);

			//	file->read_image(
			//		chbegin,
			//		chbegin + 1,
			//		OIIO::TypeDesc::HALF, // type
			//		&ptr[i * typesize], //data
			//		typesize * channels_count, // xstride
			//		typesize * channels_count * width, //ystride
			//		OIIO::AutoStride // zstride
			//	);
			//}
		}
	}
	file->close();
}