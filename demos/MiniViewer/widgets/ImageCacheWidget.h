#pragma once

#include "imgui.h"
#include "OpenImageIO/imagecache.h"

void ImageCacheWidget(const OIIO::ImageCache* image_cache) {
	
	// options
	if (ImGui::CollapsingHeader("options", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf))
	{
		int autotile;
		int autoscanline;
		int automip;
		int forcefloat;

		image_cache->getattribute("autotile", autotile);
		image_cache->getattribute("autoscanline ", autoscanline);
		image_cache->getattribute("automip", automip);
		image_cache->getattribute("forcefloat", forcefloat);

		ImGui::Text("autotile: %d", autotile);
		ImGui::Text("autoscanline: %d", autoscanline);
		ImGui::Text("automip: %d", automip);
		ImGui::Text("forcefloat: %d", forcefloat);
	}

	// files
	if (ImGui::CollapsingHeader("files", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf))
	{
		int max_open_files;
		int open_files_peak;
		int total_files;
		int open_files_current;
		image_cache->getattribute("max_open_files", max_open_files);
		image_cache->getattribute("total_files", total_files);
		image_cache->getattribute("stat:open_files_current", open_files_current);
		image_cache->getattribute("stat:open_files_peak", OIIO::TypeInt, &open_files_peak);

		ImGui::Text("files");
		ImGui::Text("max open files: %d", max_open_files);
		ImGui::Text("open_files_current: %d", open_files_current);
		ImGui::Text("total_files: %d", total_files);
		ImGui::Text("open_files_peak: %d", open_files_peak);
	}

	// memory
	if (ImGui::CollapsingHeader("memory", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf))
	{
		int64_t cache_memory_used;
		int64_t image_size;
		int64_t file_size;
		int64_t bytes_read;
		float max_memory_MB;

		image_cache->getattribute("max_memory_MB", OIIO::TypeFloat, &max_memory_MB);
		image_cache->getattribute("stat:cache_memory_used", OIIO::TypeInt64, &cache_memory_used);
		image_cache->getattribute("stat:image_size", OIIO::TypeInt64, &image_size);
		image_cache->getattribute("stat:file_size", OIIO::TypeInt64, &file_size);
		image_cache->getattribute("stat:bytes_read", OIIO::TypeInt64, &bytes_read);

		ImGui::Text("max memory: %.0f MB", max_memory_MB);
		ImGui::Text("cache memory used: %d", cache_memory_used);
		ImGui::Text("image_size: %dMB", image_size / 1024 / 1024);
		ImGui::Text("file_size: %dMB", file_size / 1024 / 1024);
		ImGui::Text("bytes_read: %d", bytes_read / 1024 / 1024);
	}

	ImGui::Text("%s", image_cache->getstats().c_str());
}