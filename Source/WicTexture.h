#pragma once

#include <memory>
#include <vector>

// Read a texture using Windows Imaging Components.
std::vector<uint32_t> ReadWicTexture(const wchar_t *inputPath, uint32_t *width, uint32_t *height);

void SaveWicTexture(const wchar_t *inputPath, uint32_t width, uint32_t height, const std::vector<uint32_t> &colors);