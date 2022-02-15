#pragma once

#include <Windows.h>
#include <functional>

#include "NTSCify\ArtifactSettings.h" 
#include "NTSCify\OverscanSettings.h" 
#include "NTSCify\ScreenSettings.h" 
#include "NTSCify\SourceSettings.h"
#include "NTSCify\TVKnobSettings.h" 


bool RunSettingsDialog(
  HWND parentWindow,
  NTSCify::SignalType *signalTypeInOut,
  NTSCify::SourceSettings *sourceSettingsInOut,
  NTSCify::ArtifactSettings *artifactSettingsInOut,
  NTSCify::TVKnobSettings *knobSettingsInOut,
  NTSCify::OverscanSettings *overscanSettingsInOut,
  NTSCify::ScreenSettings *screenSettingsInOut);