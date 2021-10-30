#pragma once

#include <memory>
#include <vector>

namespace NTSC
{
  void ReadWicTexture(const wchar_t *inputPath, std::vector<u32> *colorData, u32 *width, u32 *height);
}