#include "interactors/RenderSettingInteractor.h"
#include "interfaces/RenderSettingInteractorOutput.h"
#include "model/ProjectManager.h"

using namespace ux3d;
using namespace ux3d::editor;

RenderSettingInteractor* RenderSettingInteractor::getInstance()
{
    static RenderSettingInteractor instance;
    return &instance;
}

void RenderSettingInteractor::setIBLEnabled(bool _enabled)
{
    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentIBLEnabled(_enabled);
    }
}

void RenderSettingInteractor::setPunctualLightsEnable(bool _enabled)
{
    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentPunctualLightsEnabled(_enabled);
    }
}

void RenderSettingInteractor::setCubeMapEnable(bool _enabled)
{
    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentCubeMapEnabled(_enabled);
    }
}

void RenderSettingInteractor::cameraTypeChanged(bool _orthographic)
{
    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentCameraTypeChanged(_orthographic);
    }
}

void RenderSettingInteractor::setToneMapping(renderer::ToneMappingsEnum _toneMapping)
{
    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentToneMappingChanged(_toneMapping);
    }
}

void RenderSettingInteractor::setExposure(float _exposure)
{
    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentExposureChanged(_exposure);
    }
}

void RenderSettingInteractor::setSkinningEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentSkinningEnabled(_enabled);
    }
}

void RenderSettingInteractor::setMorphingEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentMorphingEnabled(_enabled);
    }
}

void RenderSettingInteractor::setPbrSpecularGlossinessEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentPbrSpecularGlossinessEnabled(_enabled);
    }
}
void RenderSettingInteractor::setUnlitEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentUnlitEnabled(_enabled);
    }
}

void RenderSettingInteractor::setClearCoatEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentClearCoatEnabled(_enabled);
    }
}

void RenderSettingInteractor::setSheenEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentSheenEnabled(_enabled);
    }
}

void RenderSettingInteractor::setTransmissionEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentTransmissionEnabled(_enabled);
        if (_enabled == false) {
            pOutput->presentVolumeEnabled(false);
        }
    }
}

void RenderSettingInteractor::setVolumeEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentVolumeEnabled(_enabled);
    }
}

void RenderSettingInteractor::setIridescenceEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentIridescenceEnabled(_enabled);
    }
}

void RenderSettingInteractor::setIOREnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentIOREnabled(_enabled);
    }
}

void RenderSettingInteractor::setSpecularEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentSpecularEnabled(_enabled);
    }
}

void RenderSettingInteractor::setEmissiveStrengthEnabled(bool _enabled)
{
    model::ProjectManager::stateChangedTemporarily();

    for (RenderSettingInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentEmissiveStrengthEnabled(_enabled);
    }
}
