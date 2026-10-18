#pragma once

#include <limits>
#include <stdint.h>

namespace ux3d::editor::constants {
struct Color {
    int r, g, b;
};

constexpr auto appName = UX3D_PROJECT_NAME;
constexpr auto appDescription = "A glTF editor";
constexpr auto releaseVersion = UX3D_VERSION;
constexpr auto appUri = "https://gestaltor.com";

constexpr auto unnamedGltfName = "untitled.glb";
constexpr auto unnamedSceneName = "Unnamed Scene";
constexpr auto unnamedNodeName = "Unnamed Node";
constexpr auto unnamedMaterialName = "Unnamed Material";
constexpr auto unnamedMeshName = "Unnamed Mesh";
constexpr auto unnamedCameraName = "Unnamed Camera";
constexpr auto unnamedLightName = "Unnamed Light";
constexpr auto unnamedTextureName = "Unnamed Texture";
constexpr auto unnamedImageName = "Unnamed Image";
constexpr auto unnamedSamplerName = "Unnamed Sampler";
constexpr auto unnamedSkinName = "Unnamed Skin";
constexpr auto unnamedAccessorName = "Unnamed Accessor";
constexpr auto unnamedBufferName = "Unnamed Buffer";
constexpr auto unnamedBufferViewName = "Unnamed BufferView";
constexpr auto unnamedAnimationName = "Unnamed Animation";
constexpr auto unnamedAudioSource = "Unnamed Audio Source";
constexpr auto unnamedAudioEmitter = "Unnamed Audio Emitter";

// Constants for default QPalette
constexpr auto defaultBackgroundColor = "#252525";
constexpr auto defaultTextColor = "#E1E1E1";
constexpr auto defaultDisabledTextColor = "#939393";
constexpr auto defaultBaseColor = "#545454";
constexpr auto defaultButtonColor = "#6B6B6B";

// Constants for extensionLabel
constexpr auto KHRLabelBodyColor = "#7CB41E";
constexpr auto KHRLabelTextColor = "#FFFFFF";
constexpr auto KHRLabelOutlineColor = "#7CB41E";
constexpr auto KHRLabelBodyDisabledColor = "#708C40";
constexpr auto KHRLabelTextDisabledColor = "#B1B1B1";
constexpr auto KHRLabelOutlineDisabledColor = "#708C40";
constexpr auto EXTLabelBodyColor = "#464646";
constexpr auto EXTLabelTextColor = "#FFFFFF";
constexpr auto EXTLabelOutlineColor = "#7CB41E";
constexpr auto EXTLabelBodyDisabledColor = "#4D4D4D";
constexpr auto EXTLabelTextDisabledColor = "#B1B1B1";
constexpr auto EXTLabelOutlineDisabledColor = "#708C40";
constexpr auto KNOWNLabelBodyColor = "#363636";
constexpr auto KNOWNLabelTextColor = "#DBDBDB";
constexpr auto KNOWNLabelOutlineColor = "#363636";
constexpr auto KNOWNLabelBodyDisabledColor = "#4D4D4D";
constexpr auto KNOWNLabelTextDisabledColor = "#B1B1B1";
constexpr auto UNKNOWNLabelBodyColor = "#959595";
constexpr auto UNKNOWNLabelTextColor = "#636363";
constexpr auto UNKNOWNLabelOutlineColor = "#959595";
constexpr auto UNKNOWNLabelBodyDisabledColor = "#7C7C7C";
constexpr auto UNKNOWNLabelTextDisabledColor = "#636363";
constexpr auto UNKNOWNLabelOutlineDisabledColor = "#7C7C7C";
constexpr int extensionLabelHeightInPixel = 11;

constexpr Color colorListItemHighlight = { 139, 161, 101 };

constexpr auto redErrorColorEmptySlot = "#ed1c24";
constexpr auto redErrorTextColor = "#da3d3d";

constexpr auto customToolButtonPressedIconColor = "#424242";
constexpr auto customToolButtonDisabledIconColor = "#A4A4A4";
constexpr auto animationToolButtonPressedIconColor = "#2b2b2b";
constexpr auto unsetButtonDisabledIconColor = "#6C6C6C";

constexpr int doubleSpinBoxPrecision = std::numeric_limits<double>::digits10;
constexpr uint32_t fps = 60;
constexpr uint32_t dpiThreshold = 140;
constexpr double freeCameraPitchBias = 0.1;
constexpr float cameraMinZoom = 0.000001f;

constexpr int labelRightPadding = 15;
constexpr int pushButtonAsDropDownPadding = 5;

// These values need to be reevaluated after changes are made to the largest entry
// One needs to test how large the width of the respective widgets has to be to show everything correctly
constexpr int entryValueWidth = 400;
constexpr int popupWidth = 400;
constexpr int popupEntryWidth = 185;
constexpr int animationCreationPopupWidth = 540;
constexpr int animationCreationPopupEntryWidth = 250;
constexpr int renderPopupHeightStandin = 200;

constexpr int maximumListSize = 250;
constexpr uint32_t indent = 15;
constexpr int entryMargin = 9;
constexpr float toolButtonScaling = 0.75f;
constexpr float timelineButtonScaling = 0.5f;
constexpr float animationToolButtonScaling = 0.8f;
constexpr float unsetButtonScaling = 0.8f;

constexpr auto inspectorArrowCollapse = "QLabel{image: url(:/resources/png/Icons_Generic/Icon_Collapse.png); min-width: 8px;}";
constexpr auto inspectorArrowExpand = "QLabel{image: url(:/resources/png/Icons_Generic/Icon_Expand.png); min-width: 8px;}";
}
