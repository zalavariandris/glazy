#include <vector>
#include <string>
#include <filesystem>

#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/export.h>

#include <cassert>

#include <string>
#include <vector>
#include <sstream>

#include <numeric>

std::vector<std::string> split_string(const std::string& str, char delimiter) {
	std::vector<std::string> segments;

	std::stringstream ss(str);
	std::string segment;
	while (std::getline(ss, segment, delimiter)) segments.push_back(segment);
	return segments;
}

std::string join_string(const std::vector<std::string>& segments, const std::string& delimiter, int range_start, int range_end) {
	if (range_end < 0) range_end = segments.size();
	assert(("invalid range", range_start < range_end));
	std::string result;

	// append all +delimiter but last item
	for (auto i = range_start; i < range_end - 1; i++) {
		result += segments[i];
		result += delimiter;
	}
	// append last item
	result += segments[range_end - 1];
	return result;
}

/*
* if image has subimages
* subimage->[subimage.name, subimage.view, full_cahnnel_name]
* full_cahnnel_name->[Color.channel]
* full_cahnnel_name->[layer.channel]
* full_cahnnel_name->[layer.view.channel]
*/

/* group channels by convention for a single part
an image with the following channels 'R', 'G', 'B', 'A', 'lighting.R', 'lighting.G' will becom:
color: 0,1,2,3,
lighting: 4,5
*/

std::map<std::string, std::vector<int>> group_channels(const OIIO::ImageSpec& spec) {
	std::map<std::string, std::vector<int>> layers;
	for (auto c = 0; c < spec.nchannels; c++) {
		std::string channelname = spec.channel_name(c); // get full channel name
		if (channelname == "R" || channelname == "G" || channelname == "B" || channelname == "A") // rgba
		{
			if (!layers.contains("color")) {
				layers.at("color") = std::vector<int>();
			}
			layers.at("color").push_back(c);
			continue;
		}
		if (channelname == "Z") // depth
		{
			if (!layers.contains("depth")) {
				layers.at("depth") = std::vector<int>();
			}
			layers.at("depth").push_back(c);
			continue;
		}

		auto channel_segments = split_string(channelname, '.');
		if (channel_segments.size() == 1) // AOVs
		{
			auto layer = channel_segments[0];
			if (!layers.contains(layer)) {
				layers.at(layer).push_back(c);
			}
		}
		else if (channel_segments.size() > 1)
		{
			auto channel = channel_segments.back();
			auto layer = channelname.substr(0, channelname.size() - channel.size() - 1);
			if (!layers.contains(layer)) {
				layers[layer] = std::vector<int>();
			}
			layers[layer].push_back(c);
		}
		else {
			std::cout << "ERROR: " << "Unknown channel name formatting: " << channelname << "\n";
		}
	}
	return layers;
}

int get_nsubimages(std::filesystem::path path) {
	std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::open(path.string());
	auto restore_subimage = in->current_subimage();
	auto restore_mipmap = in->current_miplevel();

	std::map<std::string, int> subimages;
	int nsubimages = 0;
	while (in->seek_subimage(nsubimages, 0)) {
		auto spec = in->spec();
		auto name = spec.get_string_attribute("name");
		auto view = spec.get_string_attribute("view");
		subimages[name] = nsubimages;
		++nsubimages;
	}
	in->seek_subimage(restore_subimage, restore_mipmap);

	return nsubimages;
}

std::optional<std::tuple<int, int, int>> find_layer_location(const std::filesystem::path path, std::string layer) {
	if (!std::filesystem::exists(path)) return {};
	std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::open(path.string());
	auto restore_subimage = in->current_subimage();
	auto restore_mipmap = in->current_miplevel();

	// find layer in subimages
	int nsubimages = 0;
	while (in->seek_subimage(nsubimages, 0)) {
		auto spec = in->spec();
		auto name = spec.get_string_attribute("name");
		if (name == layer) {
			return std::optional<std::tuple<int, int, int>>({ nsubimages, 0, spec.nchannels });
		}
		++nsubimages;
	}
	in->seek_subimage(restore_subimage, restore_mipmap);

	if (nsubimages > 1) return {}; // could not find the layer

	// find layer in channel names
	auto spec = in->spec();
	if (layer == "color") // color
	{
		std::vector<int> indices;

		for (auto c = 0; c < spec.nchannels; c++) {
			if (spec.channel_name(c) == "R" || spec.channel_name(c) == "G" || spec.channel_name(c) == "B" || spec.channel_name(c) == "A") {
				indices.push_back(c);
			}
		}

		std::sort(indices.begin(), indices.end());

		return std::optional<std::tuple<int,int,int>>({ 0, indices[0], indices.back() });

	}
	else if (layer == "depth") // depth
	{
		if (spec.z_channel < 0) return {};
		return std::optional<std::tuple<int, int, int>>({ 0, spec.z_channel, spec.z_channel });
	}
	else // AOVs
	{
		for (auto c = 0; c < spec.nchannels; c++)
		{
			auto channelname = spec.channel_name(c);
			if (channelname == "R" || channelname == "G" || channelname == "B" || channelname == "A" || channelname == "Z") continue;
			auto channel_segments = split_string(channelname, '.');
		}
	}
}

namespace ImageIO {
	/* get layers by convention */
	std::vector<std::string> get_layers(const std::filesystem::path path) {
		if (!std::filesystem::exists(path)) return {};

		std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::open(path.string());
		auto restore_subimage = in->current_subimage();
		auto restore_mipmap = in->current_miplevel();

		std::map<std::string, int> subimages;
		int nsubimages = 0;
		while (in->seek_subimage(nsubimages, 0)) {
			auto spec = in->spec();
			auto name = spec.get_string_attribute("name");
			auto view = spec.get_string_attribute("view");
			subimages[name] = nsubimages;
			++nsubimages;
		}
		in->seek_subimage(restore_subimage, restore_mipmap);

		if (nsubimages == 1)// singlepart
		{
			auto layer_mapping = group_channels(in->spec());
			std::vector<std::string> layers;
			for (auto& [name, indices] : layer_mapping) {
				layers.push_back(name);
			}
			return  layers;
		}
		else if (nsubimages > 1) // multipart
		{
			std::map<std::string, std::tuple<int, std::vector<int>>> layer_to_indices;
			for (auto& [name, idx] : subimages) {
				if (!layer_to_indices.contains(name))
					layer_to_indices[name] = std::tuple<int, std::vector<int>>();
				// get all channels for subimage
				in->seek_subimage(idx, 0);
				std::vector<int> indices(in->spec().nchannels);
				std::iota(std::begin(indices), std::end(indices), 0);
				layer_to_indices[name] = { idx, indices };
			}

			// get keys for layer names
			std::vector<std::string> layers;
			for (auto& [name, indices] : layer_to_indices) layers.push_back(name);
			return layers;
		}
		else
		{
			std::cout << "ERROR: " << "file does not contain any images" << "\n";
			return {};
		}
	}

	/* get chanels by convention */
	std::vector<std::string> get_channels(std::filesystem::path path, std::string layer) {
		std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::open(path.string());
		auto restore_subimage = in->current_subimage();
		auto restore_mipmap = in->current_miplevel();

		std::map<std::string, int> subimages;
		int nsubimages = 0;
		while (in->seek_subimage(nsubimages, 0)) {
			auto spec = in->spec();
			auto name = spec.get_string_attribute("name");
			auto view = spec.get_string_attribute("view");
			subimages[name] = nsubimages;
			++nsubimages;
		}
		in->seek_subimage(restore_subimage, restore_mipmap);

		if (nsubimages == 1) // singlepart
		{
			std::vector<std::string> result;

			// get layer 2 channel mapping
			auto channel_mapping = group_channels(in->spec());

			// collect all channel names for this group
			auto indices = channel_mapping[layer];
			for (auto i : indices) {
				auto channelname = in->spec().channel_name(i);
				auto channelsegments = split_string(channelname, '.');
				result.push_back(channelsegments.back());
			}
			return result;
		}
		else if (nsubimages > 1) // multipart
		{
			std::vector<std::string> result;

			// find image part named 'layer'
			int nsubimages = 0;
			while (in->seek_subimage(nsubimages, 0)) {
				auto spec = in->spec();
				auto name = spec.get_string_attribute("name");
				auto view = spec.get_string_attribute("view");

				if (name == layer) break;
				++nsubimages;
			}

			// collect all channel names for this part
			for (auto c = 0; c < in->spec().nchannels; c++) {
				auto channel_name = in->spec().channel_name(c);
				result.push_back(channel_name);
			}
			return result;
		}

		return {};
	}

	/* get pixels */
	void get_pixels(std::filesystem::path path, std::string layer, float* data) {
		OIIO::ImageCache* image_cache = OIIO::ImageCache::create(true);
		int nsubimages = get_nsubimages(path);

		if (nsubimages == 1) // singlepart
		{
			std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::open(path.string());
			auto spec = in->spec();
			auto layer_mapping = group_channels(spec);
			std::vector<int> indices = layer_mapping[layer];
			std::sort(indices.begin(), indices.end());

			int xbegin = 0;
			int xend = spec.width;
			int ybegin = 0;
			int yend = spec.height;
			int zbegin = 0;
			int zend = 1;
			int chbegin = indices[0];
			int chend = indices.back();
			assert(("chend is smaller then begin", chbegin < chend));
			bool success = image_cache->get_pixels(OIIO::ustring(path.string()), 0, 0,
				/*ROI*/xbegin, xend, ybegin, yend, zbegin, zend, chbegin, chend,
				/*data description*/OIIO::TypeFloat, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride,
				/*cached channels*/chbegin, chend
			);
			if (!success) std::cout << "ERROR: can't read pixels" << "\n";
		}

		if (nsubimages > 1) // multipart
		{
			std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::open(path.string());
			int nsubimages = 0;
			while (in->seek_subimage(nsubimages, 0)) {
				if (layer == in->spec().get_string_attribute("name")) break;
				++nsubimages;
			}
			auto spec = in->spec();
			int chbegin = 0;
			int chend = spec.nchannels;
			bool success = image_cache->get_pixels(OIIO::ustring(path.string()), nsubimages, 0,
				/*ROI*/0, spec.width, 0, spec.height, 0, 1, chbegin, chend,
				/*data description*/OIIO::TypeFloat, data, OIIO::AutoStride, OIIO::AutoStride, OIIO::AutoStride,
				/*cached channels*/chbegin, chend
			);
			if (!success) std::cout << "ERROR: can't read pixels" << "\n";
		}
	}

	std::tuple<std::string, std::string, std::string> parse_channel_name(std::string channel_name, std::vector<std::string> views) {
		bool isMultiView = !views.empty();

		auto channel_segments = split_string(channel_name, '.');

		std::tuple<std::string, std::string, std::string> result; // layer.view.channel

		if (channel_segments.size() == 1) {
			std::string channel = channel_segments.back();
			bool isColor = std::string("RGBA").find(channel) == std::string::npos;
			bool isDepth = channel == "Z";
			std::string layer = "other";
			if (isColor) layer = "color";
			if (isDepth) layer = "depth";
			std::string view = isMultiView ? views[0] : "";
			return std::tuple<std::string, std::string, std::string>({ "","",channel });
		}

		if (channel_segments.size() == 2) {
			// find a view name right before the final channel name. If not found this channel is not associated with any view
			bool IsInView = std::find(channel_segments.begin(), channel_segments.end(), channel_segments.end()[-2]) != channel_segments.end();

			if (IsInView) {
				return { "", channel_segments.end()[-2], channel_segments.back() };
			}
			else {
				return { channel_segments[0], "", channel_segments.back() };
			}
		}

		if (channel_segments.size() == 3) {
			// find a view name right before the final channel name. If not found this channel is not associated with any view
			bool IsInView = std::find(channel_segments.begin(), channel_segments.end(), channel_segments.end()[-2]) != channel_segments.end();

			if (IsInView) {
				// this channel is in a view
				// this is the format descriped in oenexr docs: https://www.openexr.com/documentation/MultiViewOpenEXR.pdf
				//{layer}.{view}.{final channels}
				return { channel_segments[0], channel_segments.end()[-2], channel_segments.back() };
			}
			else {
				// this channel is not in a view, but the layer name contains a dot
				//{layer.name}.{final channels}
				auto layer = join_string(channel_segments, ".", 0, channel_segments.size() - 2);
				return { layer, "", channel_segments.back() };
			}
		}

		if (channel_segments.size() > 3) {
			// find a view name right before the final channel name. If not found this channel is not associated with any view
			bool IsInView = std::find(channel_segments.begin(), channel_segments.end(), channel_segments.end()[-2]) != channel_segments.end();

			if (IsInView) {
				auto layer = join_string(channel_segments, ".", 0, channel_segments.size() - 3);
				auto view = channel_segments.end()[-2];
				auto channel = channel_segments.end()[-1];
				return { layer, view, channel };
			}
			else {
				auto layer = join_string(channel_segments, ".", 0, channel_segments.size() - 2);
				auto view = "";
				auto channel = channel_segments.end()[-1];
				return { layer, view, channel };
			}
		}
	}
}