#pragma once

#include <memory>
#include <vector>
#include "List.h"

void ReadWicTexture(const wchar_t *inputPath, std::vector<u32> *colorData, u32 *width, u32 *height);