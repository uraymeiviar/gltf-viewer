#pragma once

#include "renderer/enums/ToneMappingsEnum.h"

namespace ux3d::editor {
class SetIBLUseCase;

class RenderSettingInteractorOutput {
public:
    virtual ~RenderSettingInteractorOutput() = default;
    virtual void presentIBLEnabled(bool _enabled) { }
    virtual void presentPunctualLightsEnabled(bool _enabled) { }
    virtual void presentCubeMapEnabled(bool _enabled) { }
    virtual void presentCameraTypeChanged(bool _orthographic) { }
    virtual void presentToneMappingChanged(renderer::ToneMappingsEnum _toneMapping) { }
    virtual void presentExposureChanged(float _exposure) { }
    virtual void presentSkinningEnabled(bool _enable) { }
    virtual void presentMorphingEnabled(bool _enable) { }
    virtual void presentPbrSpecularGlossinessEnabled(bool _enable) { }
    virtual void presentUnlitEnabled(bool _enable) { }
    virtual void presentClearCoatEnabled(bool _enable) { }
    virtual void presentSheenEnabled(bool _enable) { }
    virtual void presentTransmissionEnabled(bool _enable) { }
    virtual void presentVolumeEnabled(bool _enable) { }
    virtual void presentIridescenceEnabled(bool _enable) { }
    virtual void presentIOREnabled(bool _enable) { }
    virtual void presentSpecularEnabled(bool _enable) { }
    virtual void presentEmissiveStrengthEnabled(bool _enable) { }
};
}
