#pragma once

#include <Windows.h>
#include <functional>

#include "CathodeRetro/ArtifactSettings.h"
#include "CathodeRetro/OverscanSettings.h"
#include "CathodeRetro/ScreenSettings.h"
#include "CathodeRetro/SourceSettings.h"
#include "CathodeRetro/TVKnobSettings.h"


bool RunSettingsDialog(
  HWND parentWindow,
  CathodeRetro::SignalType *signalTypeInOut,
  CathodeRetro::SourceSettings *sourceSettingsInOut,
  CathodeRetro::ArtifactSettings *artifactSettingsInOut,
  CathodeRetro::TVKnobSettings *knobSettingsInOut,
  CathodeRetro::OverscanSettings *overscanSettingsInOut,
  CathodeRetro::ScreenSettings *screenSettingsInOut);