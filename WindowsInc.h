#pragma warning (push)
#pragma warning (disable: 4668 4917 4987 4623 4626 5027)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <WinUser.h>
#include <CommCtrl.h>

#undef GetFirstChild
#undef GetNextSibling
#undef IsMaximized
#undef IsMinimized

#pragma warning (pop)