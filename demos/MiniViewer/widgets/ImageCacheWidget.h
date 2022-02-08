#pragma once

#include "imgui.h"
#include "OpenImageIO/imagecache.h"

void ImageCacheWidget(const OIIO::ImageCache* image_cache) {
	int max_open_files;
	int autotile;
	int autoscanline;
	int automip;
	int forcefloat;

	image_cache->getattribute("max_open_files", max_open_files);
	image_cache->getattribute("autotile", autotile);
	image_cache->getattribute("autoscanline ", autoscanline);
	image_cache->getattribute("automip", automip);
	image_cache->getattribute("forcefloat", forcefloat);

	ImGui::Text("max open files: %d", max_open_files);
	ImGui::Text("autotile: %d", autotile);
	ImGui::Text("autoscanline: %d", autoscanline);
	ImGui::Text("automip: %d", automip);
	ImGui::Text("forcefloat: %d", forcefloat);

	int total_files;
	image_cache->getattribute("total_files", total_files);

	// stats
	int64_t cache_memory_used;
	int open_files_peak;
	int64_t image_size;
	int64_t file_size;
	int64_t bytes_read;
	image_cache->getattribute("stat:cache_memory_used", OIIO::TypeInt64, &cache_memory_used);
	image_cache->getattribute("stat:open_files_peak", open_files_peak);
	image_cache->getattribute("stat:image_size", OIIO::TypeInt64, &image_size);
	image_cache->getattribute("stat:file_size", OIIO::TypeInt64, &file_size);
	image_cache->getattribute("stat:bytes_read", OIIO::TypeInt64, &bytes_read);

	ImGui::Text("cache memory used: %d", cache_memory_used);
	ImGui::Text("open_files_peak: %d MV", open_files_peak/1024/1024);
	ImGui::Text("image_size: %d MB", image_size / 1024 / 1024);
	ImGui::Text("file_size: %d", file_size / 1024 / 1024);
	ImGui::Text("bytes_read: %d", bytes_read / 1024 / 1024);
}