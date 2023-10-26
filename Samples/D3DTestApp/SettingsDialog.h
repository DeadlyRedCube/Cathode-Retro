#pragma once

#include <Windows.h>
#include <functional>

#include "CathodeRetro/Settings.h"


#define WM_SETTINGS_CHANGED (WM_APP + 1)

bool RunSettingsDialog(
  HWND parentWindow,
  CathodeRetro::SignalType *signalTypeInOut,
  CathodeRetro::SourceSettings *sourceSettingsInOut,
  CathodeRetro::ArtifactSettings *artifactSettingsInOut,
  CathodeRetro::TVKnobSettings *knobSettingsInOut,
  CathodeRetro::OverscanSettings *overscanSettingsInOut,
  CathodeRetro::ScreenSettings *screenSettingsInOut);