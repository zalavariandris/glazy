#include "EXRLayerManager2.h"
#include "imgui.h"

#include "OpenEXR/ImfMultiPartInputFile.h"
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfChannelList.h>

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



EXRLayerManager2::EXRLayerManager2(const std::filesystem::path& filename)
{
	auto file = std::make_unique<Imf::MultiPartInputFile>(filename.string().c_str());

	/// Collect layers per parts
	{
		std::vector<Layer> part_layers;

		for (auto p = 0; p < file->parts(); p++)
		{
			auto header = file->header(p);
			std::string part_name = header.hasName() ? header.name() : "";
			auto cl = header.channels();

			std::vector<std::string> channel_names;
			for (Imf::ChannelList::ConstIterator c = cl.begin(); c != cl.end(); ++c) {
				channel_names.push_back(c.name());
			}

			auto layer = Layer(part_name, p, channel_names);
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
				size_t found = channel.find_last_of(".");
				auto viewlayer = (found != std::string::npos) ? channel.substr(0, found) : "";
				auto channel_name = channel.substr(found + 1);

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
				auto [_, channel_name] = split_channel_id(channel);
				channel_names.push_back(channel_name);
			}

			// enumerate channel groups
			for (const auto& [begin, end] : group_stringvector_by_patterns(channel_names, patterns))
			{
				auto channels_by_pattern = std::vector<std::string>(layer.channels.begin() + begin, layer.channels.begin() + end);
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

bool EXRLayerManager2::onGUI()
{
	bool changed = false;

	// Current channels
	if (mSelectedLayer != NULL) {
		ImGui::LabelText("current part", "%d", mSelectedLayer->part);
		for (const auto& channel : mSelectedLayer->channels) {
			ImGui::Text(channel.c_str());
			ImGui::SameLine();
		}
		ImGui::NewLine();
	}

	/// All channels
	if (ImGui::BeginTabBar("EXRLayerManager-tabs"))
	{
		if (ImGui::BeginTabItem("Layers"))
		{
			if (ImGui::BeginTable("Layers-table", 3)) {
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("subimage");
				ImGui::TableSetupColumn("channels");
				ImGui::TableHeadersRow();

				for (int i = 0; i < mLayers.size(); i++)
				{
					const auto& layer = mLayers.at(i);
					ImGui::PushID(i);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if (ImGui::Selectable(layer.name.c_str(), &layer == mSelectedLayer, ImGuiSelectableFlags_SpanAllColumns)) {
						set_selected_layer(i);
						changed = true;
					}
					ImGui::TableNextColumn();
					ImGui::Text("%d", layer.part);
					ImGui::TableNextColumn();
					for (const auto& channel : layer.channels) {
						ImGui::Text("%s", channel.c_str());
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