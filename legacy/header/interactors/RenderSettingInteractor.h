#pragma once

#include "enums/InteractorResult.h"
#include "renderer/enums/ToneMappingsEnum.h"
#include <list>

namespace ux3d::editor {
class RenderSettingInteractorOutput;

class RenderSettingInteractor {
public:
    static RenderSettingInteractor* getInstance();
    ~RenderSettingInteractor() = default;
    RenderSettingInteractor(RenderSettingInteractor const&) = delete;
    void operator=(RenderSettingInteractor const&) = delete;

    void registerOutput(RenderSettingInteractorOutput* _NewOutput) { m_Outputs.push_back(_NewOutput); }
    void unregisterOutput(RenderSettingInteractorOutput* _RemoveOutput) { m_Outputs.remove(_RemoveOutput); }

    void setIBLEnabled(bool _enabled);
    void setPunctualLightsEnable(bool _enabled);
    void setCubeMapEnable(bool _enable);
    void cameraTypeChanged(bool _orthographic);
    void setToneMapping(renderer::ToneMappingsEnum _toneMapping);
    void setExposure(float _exposure);
    void setSkinningEnabled(bool _enabled);
    void setMorphingEnabled(bool _enabled);
    void setPbrSpecularGlossinessEnabled(bool _enabled);
    void setUnlitEnabled(bool _enabled);
    void setClearCoatEnabled(bool _enabled);
    void setSheenEnabled(bool _enabled);
    void setTransmissionEnabled(bool _enabled);
    void setVolumeEnabled(bool _enabled);
    void setIridescenceEnabled(bool _enabled);
    void setIOREnabled(bool _enabled);
    void setSpecularEnabled(bool _enabled);
    void setEmissiveStrengthEnabled(bool _enabled);

private:
    RenderSettingInteractor() = default;
    std::list<RenderSettingInteractorOutput*> m_Outputs;
};
}
