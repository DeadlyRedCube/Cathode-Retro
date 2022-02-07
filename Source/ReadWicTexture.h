#pragma once

#include <memory>

#include "SimpleArray.h"

SimpleArray<uint32_t> ReadWicTexture(const wchar_t *inputPath, uint32_t *width, uint32_t *height);
