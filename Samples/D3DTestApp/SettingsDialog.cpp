#include <Windows.h>
#include <CommCtrl.h>
#include <cinttypes>
#include <functional>

#include "resource.h"
#include "SettingsDialog.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

template <typename T>
constexpr auto EnumValue(T v)
{
  return static_cast<std::underlying_type_t<T>>(v);
}


// Get the length of an array from its type
template <typename T> size_t k_arrayLength = 0;
template <typename T> size_t k_arrayLength<T &> = k_arrayLength<T>;
template <typename T, size_t N> size_t k_arrayLength<T[N]> = N;


// This file is a whole bunch of garbage code to run the dialog. I'm not proud of it, but it took like 3 hours to make and it works enough.

template <typename Type>
class Slider
{
public:
  Slider() = default;
  Slider(
    HWND dialogIn,
    uint32_t sliderIDIn,
    uint32_t labelIDIn,
    Type *valuePtrIn,
    Type minValueIn,
    Type maxValueIn,
    uint32_t stepCountIn,
    std::function<void()> valueChangedCallbackIn)
  : dialog(dialogIn)
  , sliderWindow(GetDlgItem(dialogIn, sliderIDIn))
  , sliderID(sliderIDIn)
  , labelID(labelIDIn)
  , valuePtr(valuePtrIn)
  , initialValue(*valuePtrIn)
  , minValue(minValueIn)
  , maxValue(maxValueIn)
  , stepCount(stepCountIn)
  , valueChangedCallback(valueChangedCallbackIn)
  {
    SendDlgItemMessage(dialog, sliderID, TBM_SETRANGE, TRUE, MAKELPARAM(0, stepCount - 1));
    SetValue(initialValue);
    SetWindowSubclass(dialog, StaticSubclassProc, UINT_PTR(this), DWORD_PTR(this));
  }

  Slider(const Slider &) = default;
  Slider & operator=(const Slider &) = default;

  Slider &operator=(Slider &&moveFrom)
  {
    // Do a non-copy equality first
    *this = moveFrom;

    // Remove the sublass from the move source and clear its dialog
    RemoveWindowSubclass(dialog, StaticSubclassProc, UINT_PTR(&moveFrom));
    moveFrom.dialog = nullptr;

    // Set the subclass on US instead
    SetWindowSubclass(dialog, StaticSubclassProc, UINT_PTR(this), DWORD_PTR(this));
    return *this;
  }

  ~Slider()
  {
    if (dialog != nullptr)
    {
      RemoveWindowSubclass(dialog, StaticSubclassProc, UINT_PTR(this));
    }
  }

  Type GetValue()
  {
    auto valueFromSlider = SendDlgItemMessage(dialog, sliderID, TBM_GETPOS, 0, 0);
    double scaled = double(valueFromSlider) * double(maxValue - minValue) / double(stepCount - 1);
    return Type(scaled) + minValue;
  }

  Type GetInitialValue()
  {
    return initialValue;
  }

  void SetValue(Type val)
  {
    double scaled = double(val - minValue) * double(stepCount - 1) / double(maxValue - minValue);
    SendDlgItemMessage(dialog, sliderID, TBM_SETPOS, TRUE, LPARAM(std::round(scaled)));
    UpdateLabel();
  }

private:
  LRESULT SubclassProc(uint32_t message, WPARAM wparam, LPARAM lparam)
  {
    switch (message)
    {
    case WM_HSCROLL:
      if (HWND(lparam) == sliderWindow)
      {
        *valuePtr = GetValue();
        valueChangedCallback();
        UpdateLabel();
      }
      break;
    }

    return DefSubclassProc(dialog, message, wparam, lparam);
  }

  static LRESULT __stdcall StaticSubclassProc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    UINT_PTR,
    DWORD_PTR windowBase)
  {
    auto slider = reinterpret_cast<Slider *>(windowBase);
    if (slider != nullptr)
    {
      return slider->SubclassProc(message, wparam, lparam);
    }

    return DefSubclassProc(window, message, wparam, lparam);
  }


  void UpdateLabel()
  {
    char buf[512];
    static_assert(std::is_same_v<float, Type> || std::is_same_v<uint32_t, Type>);
    if constexpr (std::is_same_v<float, Type>)
    {
      sprintf_s(buf, "%.02f", GetValue());
    }
    else if constexpr (std::is_same_v<uint32_t, Type>)
    {
      sprintf_s(buf, "%u", GetValue());
    }

    SetDlgItemTextA(dialog, labelID, buf);
  }

  HWND dialog;
  HWND sliderWindow;
  uint32_t sliderID;
  uint32_t labelID;

  Type *valuePtr = nullptr;
  Type initialValue;
  Type minValue;
  Type maxValue;
  uint32_t stepCount;
  std::function<void()> valueChangedCallback;
};


class SettingsDialog
{
public:
  SettingsDialog(
    HWND parentIn,
    NTSCify::SignalType *signalTypeInOut,
    NTSCify::SourceSettings *sourceSettingsInOut,
    NTSCify::ArtifactSettings *artifactSettingsInOut,
    NTSCify::TVKnobSettings *knobSettingsInOut,
    NTSCify::OverscanSettings *overscanSettingsInOut,
    NTSCify::ScreenSettings *screenSettingsInOut)
  : parent(parentIn)
  , signalType(signalTypeInOut)
  , sourceSettings(sourceSettingsInOut)
  , artifactSettings(artifactSettingsInOut)
  , knobSettings(knobSettingsInOut)
  , overscanSettings(overscanSettingsInOut)
  , screenSettings(screenSettingsInOut)
  , initialSignalType(*signalTypeInOut)
  , initialSourceSettings(*sourceSettingsInOut)
  , initialArtifactSettings(*artifactSettingsInOut)
  , initialKnobSettings(*knobSettingsInOut)
  , initialOverscanSettings(*overscanSettingsInOut)
  , initialScreenSettings(*screenSettingsInOut)
  {
  }

  bool Run()
  {
    auto result = DialogBoxParam(
      GetModuleHandle(nullptr),
      MAKEINTRESOURCE(IDD_DISPLAY_SETTINGS_DIALOG),
      parent,
      StaticDialogProc,
      reinterpret_cast<LPARAM>(this));
    return result == IDOK;
  }


private:
  INT_PTR DialogProc(UINT message, WPARAM wparam, LPARAM)
  {
    switch (message)
    {
    case WM_INITDIALOG:
      for (const auto &preset : NTSCify::k_artifactPresets)
      {
        SendDlgItemMessageA(dialog, IDC_ARTIFACT_PRESET, CB_ADDSTRING, 0, LPARAM(preset.name));
      }

      for (const auto &preset : NTSCify::k_screenPresets)
      {
        SendDlgItemMessageA(dialog, IDC_SCREEN_PRESET, CB_ADDSTRING, 0, LPARAM(preset.name));
      }

      for (const auto &preset : NTSCify::k_sourcePresets)
      {
        SendDlgItemMessageA(dialog, IDC_SIGNAL_TIMING, CB_ADDSTRING, 0, LPARAM(preset.name));
      }

      SendDlgItemMessageA(dialog, IDC_SIGNAL_TYPE, CB_ADDSTRING, 0, LPARAM("RGB (No Signal Processing)"));
      SendDlgItemMessageA(dialog, IDC_SIGNAL_TYPE, CB_ADDSTRING, 0, LPARAM("SVideo"));
      SendDlgItemMessageA(dialog, IDC_SIGNAL_TYPE, CB_ADDSTRING, 0, LPARAM("Composite"));

      UpdateSliders();
      break;

    case WM_COMMAND:
      switch (LOWORD(wparam))
      {
      case IDOK:
        EndDialog(dialog, IDOK);
        break;

      case IDCANCEL:
        *signalType = initialSignalType;
        *sourceSettings = initialSourceSettings;
        *artifactSettings = initialArtifactSettings;
        *knobSettings = initialKnobSettings;
        *overscanSettings = initialOverscanSettings;
        *screenSettings = initialScreenSettings;
        EndDialog(dialog, IDCANCEL);
        break;

      case IDC_SIGNAL_TYPE:
        if (HIWORD(wparam) == CBN_SELCHANGE)
        {
          int32_t sel = uint32_t(SendDlgItemMessage(dialog, IDC_SIGNAL_TYPE, CB_GETCURSEL, 0, 0));
          if (sel >= 0)
          {
            *signalType = NTSCify::SignalType(sel);
          }
          UpdateSliders();
        }

        break;

      case IDC_SIGNAL_TIMING:
        if (HIWORD(wparam) == CBN_SELCHANGE)
        {
          uint32_t sel = uint32_t(SendDlgItemMessage(dialog, IDC_SIGNAL_TIMING, CB_GETCURSEL, 0, 0));
          if (sel < k_arrayLength<decltype(NTSCify::k_sourcePresets)>)
          {
            *sourceSettings = NTSCify::k_sourcePresets[sel].settings;
            UpdateSliders();
          }
        }

        break;

      case IDC_ARTIFACT_PRESET:
        if (HIWORD(wparam) == CBN_SELCHANGE)
        {
          uint32_t sel = uint32_t(SendDlgItemMessage(dialog, IDC_ARTIFACT_PRESET, CB_GETCURSEL, 0, 0));
          if (sel < k_arrayLength<decltype(NTSCify::k_artifactPresets)>)
          {
            *artifactSettings = NTSCify::k_artifactPresets[sel].settings;
            UpdateSliders();
          }
        }

        break;

      case IDC_SCREEN_PRESET:
        if (HIWORD(wparam) == CBN_SELCHANGE)
        {
          uint32_t sel = uint32_t(SendDlgItemMessage(dialog, IDC_SCREEN_PRESET, CB_GETCURSEL, 0, 0));
          if (sel < k_arrayLength<decltype(NTSCify::k_screenPresets)>)
          {
            *screenSettings = NTSCify::k_screenPresets[sel].settings;
            UpdateSliders();
          }
        }

        break;
      }

    default:
      return FALSE;
    }

    return TRUE;
  }

  static INT_PTR CALLBACK StaticDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
  {
    SettingsDialog *self = nullptr;
    if (message == WM_INITDIALOG)
    {
      self = reinterpret_cast<SettingsDialog *>(lparam);
      self->dialog = dialog;
      SetWindowLongPtr(dialog, DWLP_USER, lparam);
    }
    else
    {
      self = reinterpret_cast<SettingsDialog *>(GetWindowLongPtr(dialog, DWLP_USER));
    }

    if (self != nullptr && self->dialog != nullptr)
    {
      return self->DialogProc(message, wparam, lparam);
    }

    return FALSE;
  }
  void UpdateDisplay()
  {
    SendDlgItemMessage(dialog, IDC_SIGNAL_TYPE, CB_SETCURSEL, WPARAM(EnumValue(*signalType)), 0);
    {
      bool found = false;
      for (uint32_t i = 0; i < k_arrayLength<decltype(NTSCify::k_sourcePresets)>; i++)
      {
        if (*sourceSettings == NTSCify::k_sourcePresets[i].settings)
        {
          SendDlgItemMessage(dialog, IDC_SIGNAL_TIMING, CB_SETCURSEL, WPARAM(i), 0);
          found = true;
          break;
        }
      }

      if (!found)
      {
        SendDlgItemMessage(dialog, IDC_SIGNAL_TIMING, CB_SETCURSEL, WPARAM(-1), 0);
      }
    }

    {
      bool found = false;
      for (uint32_t i = 0; i < k_arrayLength<decltype(NTSCify::k_artifactPresets)>; i++)
      {
        if (*artifactSettings == NTSCify::k_artifactPresets[i].settings)
        {
          SendDlgItemMessage(dialog, IDC_ARTIFACT_PRESET, CB_SETCURSEL, WPARAM(i), 0);
          found = true;
          break;
        }
      }

      if (!found)
      {
        SendDlgItemMessage(dialog, IDC_ARTIFACT_PRESET, CB_SETCURSEL, WPARAM(-1), 0);
      }
    }

    {
      bool found = false;
      for (uint32_t i = 0; i < k_arrayLength<decltype(NTSCify::k_screenPresets)>; i++)
      {
        if (*screenSettings == NTSCify::k_screenPresets[i].settings)
        {
          SendDlgItemMessage(dialog, IDC_SCREEN_PRESET, CB_SETCURSEL, WPARAM(i), 0);
          found = true;
          break;
        }
      }

      if (!found)
      {
        SendDlgItemMessage(dialog, IDC_SCREEN_PRESET, CB_SETCURSEL, WPARAM(-1), 0);
      }
    }
  }

  void UpdateSliders()
  {
    noiseStrengthSlider = {
      dialog,
      IDC_NOISE_STRENGTH_SLIDER,
      IDC_NOISE_STRENGTH_LABEL,
      &artifactSettings->noiseStrength,
      0.0f,
      1.0f,
      41,
      [this]() { UpdateDisplay(); }};
    pictureInstabilitySlider = {
      dialog,
      IDC_INSTABILITY_SLIDER,
      IDC_INSTABILITY_LABEL,
      &artifactSettings->instabilityScale,
      0.0f,
      5.0f,
      41,
      [this]() { UpdateDisplay(); }};
    ghostVisibilitySlider = {
      dialog,
      IDC_GHOST_VISIBILITY_SLIDER,
      IDC_GHOST_VISIBILITY_LABEL,
      &artifactSettings->ghostVisibility,
      0.0f,
      1.0f,
      21,
      [this]() { UpdateDisplay(); }};
    ghostBlurWidthSlider = {
      dialog,
      IDC_GHOST_BLUR_WIDTH_SLIDER,
      IDC_GHOST_BLUR_WIDTH_LABEL,
      &artifactSettings->ghostSpreadScale,
      0.0f,
      5.0f,
      51,
      [this]() { UpdateDisplay(); }};
    ghostDistanceSlider = {
      dialog,
      IDC_GHOST_DISTANCE_SLIDER,
      IDC_GHOST_DISTANCE_LABEL,
      &artifactSettings->ghostDistance,
      0.0f,
      10.0f,
      51,
      [this]() { UpdateDisplay(); }};
    temporalArtifactReductionSlider = {
      dialog,
      IDC_ARTIFACT_REDUCTION_SLIDER,
      IDC_ARTIFACT_REDUCTION_LABEL,
      &artifactSettings->temporalArtifactReduction,
      0.0f,
      1.0f,
      50,
      [this]() { UpdateDisplay(); }};
    tintSlider = {
      dialog,
      IDC_TINT_SLIDER,
      IDC_TINT_LABEL,
      &knobSettings->tint,
      -0.5f,
      0.5f,
      41,
      [this]() { UpdateDisplay(); }};
    saturationSlider = {
      dialog,
      IDC_SATURATION_SLIDER,
      IDC_SATURATION_LABEL,
      &knobSettings->saturation,
      0.0f,
      1.0f,
      20,
      [this]() { UpdateDisplay(); }};
    brightnessSlider = {
      dialog,
      IDC_BRIGHTNESS_SLIDER,
      IDC_BRIGHTNESS_LABEL,
      &knobSettings->brightness,
      0.0f,
      1.0f,
      20,
      [this]() { UpdateDisplay(); }};
    sharpnessSlider = {
      dialog,
      IDC_SHARPNESS_SLIDER,
      IDC_SHARPNESS_LABEL,
      &knobSettings->sharpness,
      -1.0f
      , 1.0f
      , 41
      , [this]() { UpdateDisplay(); }};
    hDistortionSlider = {
      dialog,
      IDC_H_DISTORTION_SLIDER,
      IDC_H_DISTORTION_LABEL,
      &screenSettings->horizontalDistortion,
      0.0f,
      0.6f,
      13,
      [this]() { UpdateDisplay(); }};
    vDistortionSlider = {
      dialog,
      IDC_V_DISTORTION_SLIDER,
      IDC_V_DISTORTION_LABEL,
      &screenSettings->verticalDistortion,
      0.0f,
      0.6f,
      13,
      [this]() { UpdateDisplay(); }};
    cornerRoundnessSlider = {
      dialog,
      IDC_CORNER_ROUNDNESS_SLIDER,
      IDC_CORNER_ROUNDNESS_LABEL,
      &screenSettings->cornerRounding,
      0.0f,
      0.2f,
      9,
      [this]() { UpdateDisplay(); }};
    lrEdgeRoundingSlider = {
      dialog,
      IDC_LR_EDGE_ROUNDING_SLIDER,
      IDC_LR_EDGE_ROUNDING_LABEL,
      &screenSettings->screenEdgeRoundingX,
      0.0f,
      0.45f,
      10,
      [this]() { UpdateDisplay(); }};
    tbEdgeRoundingSlider = {
      dialog,
      IDC_TB_EDGE_ROUNDING_SLIDER,
      IDC_TB_EDGE_ROUNDING_LABEL,
      &screenSettings->screenEdgeRoundingY,
      0.0f,
      0.45f,
      10,
      [this]() { UpdateDisplay(); }};
    overscanLeftSlider = {
      dialog,
      IDC_OVERSCAN_LEFT_SLIDER,
      IDC_OVERSCAN_LEFT_LABEL,
      &overscanSettings->overscanLeft,
      0U,
      32U,
      32,
      [this]() { UpdateDisplay(); }};
    overscanRightSlider = {
      dialog,
      IDC_OVERSCAN_RIGHT_SLIDER,
      IDC_OVERSCAN_RIGHT_LABEL,
      &overscanSettings->overscanRight,
      0U,
      32U,
      32,
      [this]() { UpdateDisplay(); }};
    overscanTopSlider = {
      dialog,
      IDC_OVERSCAN_TOP_SLIDER,
      IDC_OVERSCAN_TOP_LABEL,
      &overscanSettings->overscanTop,
      0U,
      32U,
      32,
      [this]() { UpdateDisplay(); }};
    overscanBottomSlider = {
      dialog,
      IDC_OVERSCAN_BOTTOM_SLIDER,
      IDC_OVERSCAN_BOTTOM_LABEL,
      &overscanSettings->overscanBottom,
      0U,
      32U,
      32,
      [this]() { UpdateDisplay(); }};
    shadowMaskScaleSlider = {
      dialog,
      IDC_SHADOW_MASK_SCALE_SLIDER,
      IDC_SHADOW_MASK_SCALE_LABEL,
      &screenSettings->shadowMaskScale,
      0.8f,
      2.0f,
      44,
      [this]() { UpdateDisplay(); }};
    shadowMaskStrengthSlider = {
      dialog,
      IDC_SHADOW_MASK_STRENGTH_SLIDER,
      IDC_SHADOW_MASK_STRENGTH_LABEL,
      &screenSettings->shadowMaskStrength,
      0.0f,
      1.0f,
      20,
      [this]() { UpdateDisplay(); }};
    scanlineStrengthSlider = {
      dialog,
      IDC_SCANLINE_STRENGTH_SLIDER,
      IDC_SCANLINE_STRENGTH_LABEL,
      &screenSettings->scanlineStrength,
      0.0f,
      1.0f,
      20,
      [this]() { UpdateDisplay(); }};
    phosphorPersistenceSlider = {
      dialog,
      IDC_PHOSPHOR_PERSISTENCE_SLIDER,
      IDC_PHOSPHOR_PERSISTENCE_LABEL,
      &screenSettings->phosphorPersistence,
      0.0f,
      0.9f,
      19,
      [this]() { UpdateDisplay(); }};
    diffusionSlider = {
      dialog,
      IDC_DIFFUSION_SLIDER,
      IDC_DIFFUSION_LABEL,
      &screenSettings->diffusionStrength,
      0.0f,
      0.9f,
      19,
      [this]() { UpdateDisplay(); }};
    UpdateDisplay();
  }

  HWND parent = nullptr;
  HWND dialog = nullptr;
  NTSCify::SignalType *signalType;
  NTSCify::SourceSettings *sourceSettings;
  NTSCify::ArtifactSettings *artifactSettings;
  NTSCify::TVKnobSettings *knobSettings;
  NTSCify::OverscanSettings *overscanSettings;
  NTSCify::ScreenSettings *screenSettings;

  NTSCify::SignalType initialSignalType;
  NTSCify::SourceSettings initialSourceSettings;
  NTSCify::ArtifactSettings initialArtifactSettings;
  NTSCify::TVKnobSettings initialKnobSettings;
  NTSCify::OverscanSettings initialOverscanSettings;
  NTSCify::ScreenSettings initialScreenSettings;


  Slider<float> noiseStrengthSlider;
  Slider<float> pictureInstabilitySlider;
  Slider<float> ghostVisibilitySlider;
  Slider<float> ghostBlurWidthSlider;
  Slider<float> ghostDistanceSlider;
  Slider<float> temporalArtifactReductionSlider;
  Slider<float> tintSlider;
  Slider<float> saturationSlider;
  Slider<float> brightnessSlider;
  Slider<float> sharpnessSlider;
  Slider<float> hDistortionSlider;
  Slider<float> vDistortionSlider;
  Slider<float> cornerRoundnessSlider;
  Slider<float> lrEdgeRoundingSlider;
  Slider<float> tbEdgeRoundingSlider;
  Slider<uint32_t> overscanLeftSlider;
  Slider<uint32_t> overscanRightSlider;
  Slider<uint32_t> overscanTopSlider;
  Slider<uint32_t> overscanBottomSlider;
  Slider<float> shadowMaskScaleSlider;
  Slider<float> shadowMaskStrengthSlider;
  Slider<float> scanlineStrengthSlider;
  Slider<float> phosphorPersistenceSlider;
  Slider<float> diffusionSlider;
};


bool RunSettingsDialog(
  HWND parentWindow,
  NTSCify::SignalType *signalTypeInOut,
  NTSCify::SourceSettings *sourceSettingsInOut,
  NTSCify::ArtifactSettings *artifactSettingsInOut,
  NTSCify::TVKnobSettings *knobSettingsInOut,
  NTSCify::OverscanSettings *overscanSettingsInOut,
  NTSCify::ScreenSettings *screenSettingsInOut)
{
  SettingsDialog dlg{
    parentWindow,
    signalTypeInOut,
    sourceSettingsInOut,
    artifactSettingsInOut,
    knobSettingsInOut,
    overscanSettingsInOut,
    screenSettingsInOut};
  return dlg.Run();
}