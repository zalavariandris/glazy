#include "OIIOLayerManager.h"
#include "imgui.h"
#include <string>

inline std::tuple<std::string, std::string> split_channel_id(const std::string& channel_id)
{
	// find the last delimiter positions
	size_t found = channel_id.find_last_of(".");
	// if no delimiter, than viewlayer is an empty string
	// othervise its the part before the last delimiter
	auto viewlayer = (found != std::string::npos) ? channel_id.substr(0, found) : "";

	auto channel = channel_id.substr(found + 1);
	return { viewlayer, channel };
}

inline std::vector<std::tuple<int, int>> group_stringvector_by_patterns(const std::vector<std::string>& statement, const std::vector<std::vector<std::string>>& patterns)
{
	std::vector<std::tuple<int, int>> groups;

	// sort patterns
	auto sorted_patterns = patterns;
	std::sort(sorted_patterns.begin(), sorted_patterns.end(), [](const std::vector<std::string>& A, const std::vector<std::string>& B) {
		return A.size() > B.size();
		});

	// collect pattern matches
	int i = 0;
	while (i < statement.size())
	{
		std::optional<std::tuple<int, int>> match;

		// check all patterns at current index
		// if match is found, move iterator to the end of match
		for (std::vector<std::string> pattern : sorted_patterns) {
			auto begin = i;
			auto end = i + pattern.size();
			auto statement_sample = std::vector<std::string>(statement.begin() + begin, statement.begin() + std::min(end, statement.size()));
			bool IsMatch = pattern == statement_sample;
			if (IsMatch)
			{
				match = std::tuple<int, int>(begin, end);
				i = end - 1;
				break;
			}
		}

		// if match is found, add it to the stack
		// otherwise, keep single item 
		if (match)
		{
			groups.push_back(match.value());
		}
		else {
			groups.push_back(std::tuple<int, int>(i, i + 1));
		}
		i++;
	}
	return groups;
}

OIIOLayerManager::OIIOLayerManager(const std::filesystem::path& filename) {
	mFilename = filename;
	file =  OIIO::ImageInput::open(filename.string());
	
	/// Collect layers per subimage
	{
		std::vector<Layer> part_layers;
		int subimage = 0;
		while (file->seek_subimage(subimage, 0)) {
			const auto& spec = file->spec();
			std::vector<Channel> channels;
			for (int channel = 0; channel < spec.nchannels; channel++)
			{
				std::string channel_name = spec.channel_name(channel);
				channels.push_back({ channel, subimage, channel_name });
			}
			auto name = spec.get_string_attribute("name");
			auto layer = Layer(name, subimage, channels);
			layer.part = subimage;

			subimage++;

			part_layers.push_back(layer);
		}
		this->mLayers = part_layers;
	}

	/// group layers by delimiter
	{
		std::vector<Layer> layers_by_delimiter;
		for (auto& layer : this->mLayers)
		{
			std::vector<Layer> child_layers;
			for (const auto& channel : layer.channels)
			{
				size_t found = channel.name.find_last_of(".");
				auto viewlayer = (found != std::string::npos) ? channel.name.substr(0, found) : "";
				auto channel_name = channel.name.substr(found + 1);

				if (child_layers.empty() || child_layers.back().name != viewlayer)
				{
					child_layers.push_back(Layer(viewlayer, layer.part, {}));
				}

				child_layers.back().channels.push_back(channel);
			}
			// append children layer
			for (const auto& layer : child_layers) {
				layers_by_delimiter.push_back(layer);
			}

			//layer.channels = std::vector<Channel>();
			//layer.children = child_layers;
		}
		this->mLayers = layers_by_delimiter;
	}

	/// group layers by patterns
	{
		const std::vector<std::vector<std::string>> patterns = { 
			{"red", "green", "blue"},
			{"R", "G", "B", "A"}, {"R", "G", "B"}, {"R", "G"},
			{"A", "B", "G", "R"},{"B", "G", "R"}, {"G", "R"},
			{"x", "y", "z"}, {"x", "y"},
			{"u", "v", "w"}, {"u", "v"}
		};
		std::vector<Layer> layers_by_patterns;
		for (const auto& layer : this->mLayers)
		{
			std::vector<Layer> child_layers;
			std::vector<std::string> channel_names;
			for (const auto& channel : layer.channels)
			{
				auto [_, channel_name] = split_channel_id(channel.name);
				channel_names.push_back(channel_name);
			}

			// enumerate channel groups
			for (const auto& [begin, end] : group_stringvector_by_patterns(channel_names, patterns))
			{
				auto channels_by_pattern = std::vector<Channel>(layer.channels.begin() + begin, layer.channels.begin() + end);
				child_layers.push_back({ layer.name, layer.part, channels_by_pattern });
			}

			// append children layer
			for (const auto& layer : child_layers) {
				layers_by_patterns.push_back(layer);
			}
		}
		this->mLayers = layers_by_patterns;
	}
}

bool OIIOLayerManager::onGUI()
{
	bool changed = false;

	/// Properties
	ImGui::LabelText("filename", "%s", mFilename.string().c_str());
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(mFilename.string().c_str());
	}

	// Current channels
	if (selected_layer != NULL) {
		ImGui::LabelText("current part", "%d", selected_layer->part);
		for (const auto& channel : selected_channels()) {
			ImGui::Text(channel.c_str());
		}
	}

	/// All channels
	if (ImGui::BeginTabBar("OIIOLayerManager-tabs"))
	{
		if (ImGui::BeginTabItem("Layers"))
		{
			if (ImGui::BeginTable("Layers-table", 3)) {
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("subimage");
				ImGui::TableSetupColumn("channels");
				ImGui::TableHeadersRow();

				for (int i=0;i<mLayers.size();i++)
				{
					const auto& layer = mLayers.at(i);
					ImGui::PushID(i);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if (ImGui::Selectable(layer.name.c_str(), &layer == selected_layer, ImGuiSelectableFlags_SpanAllColumns)) {
						selected_layer = &layer;
						changed = true;
					}
					ImGui::TableNextColumn();
					ImGui::Text("%d", layer.part);
					ImGui::TableNextColumn();
					for (const auto& channel : layer.channels) {
						ImGui::Text("%s", channel.name.c_str());
						ImGui::SameLine();
					}
					ImGui::NewLine();

					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	return changed;
}