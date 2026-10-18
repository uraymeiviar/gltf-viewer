#include "widgets/ViewportSettings.h"
#include "HotReloadQQuickWidget.h"
#include "Preferences.h"
#include "core/Platform.h"
#include "helpers/QtFileSystem.h"
#include "helpers/Utils.h"
#include "interactors/GltfInteractor.h"
#include "interactors/LoadInteractor.h"
#include "interactors/RenderSettingInteractor.h"
#include "model/IblSamplerTaskBinding.h"
#include "model/ProjectManager.h"
#include "useCases/GltfUseCases.h"
#include "widgets/GLTFEditorWidget.h"
#include "widgets/MainWindow.h"
#include <QFileDialog>
#include <QQuickItem>
#include <QStandardPaths>
#include <QUrl>
#include <optional>

using namespace ux3d;
using namespace ux3d::editor;

ViewportSettings::ViewportSettings(QWidget* parent, QQmlEngine* engine)
    : CustomDockWidget(parent)
    , m_environmentMaps(filesystem::Path(QtFileSystem::instance(), QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first().toStdString()) / "environments", this)
    , m_defaultEnvironmentMap(filesystem::Path(QtFileSystem::instance(), Preferences::getInstance()->getEnvironmentDirectory().toStdString()))
    , m_iblSamplerTaskBinding(0)
    , m_pMaterialVariantsList(this)
{
    setObjectName("ViewportSettingsDockWidget");

    LoadInteractor::getInstance()->registerOutput(this);
    RenderSettingInteractor::getInstance()->registerOutput(this);
    GltfInteractor::getInstance()->registerOutput(this);

    m_hdrLoadTimer = new QTimer(this);
    connect(m_hdrLoadTimer, &QTimer::timeout, this, [&]() { m_iblSamplerTaskBinding.refresh(); });
    connect(this, &QDockWidget::topLevelChanged, this, &ViewportSettings::topLevelChanged);

    m_iblSamplerTaskCollectionBinding = model::IblSamplerTaskCollectionBinding();
    m_iblSamplerTaskCollectionBinding.bind(this);

    m_qmlWidget = new HotReloadQQuickWidget(engine, this);
    m_qmlWidget->setRootContext("viewportSettings", this);
    m_qmlWidget->setSource(QUrl(QString(UX3D_QML_ROOT_DIR_URL) + "/viewportSettings.qml"));
    m_qmlWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlWidget->setMinimumWidth(400);
    this->setWidget(m_qmlWidget);

    m_environmentMaps.refresh();
    m_pMaterialVariantsList.refresh();

    // initialize background color from the preferences
    m_backgroundColor = Preferences::getInstance()->getClearColor();
    emit backgroundColorChanged();
}

ViewportSettings::~ViewportSettings()
{
    LoadInteractor::getInstance()->unregisterOutput(this);
    RenderSettingInteractor::getInstance()->unregisterOutput(this);
    GltfInteractor::getInstance()->unregisterOutput(this);
}

void ViewportSettings::topLevelChanged(bool topLevel)
{
    m_environmentMaps.refresh();
    m_pMaterialVariantsList.refresh();

    // we move our environment maps by calling  m_environmentMaps.refresh()
    // This means, the pointer currentEnvironmentMap is now invalid
    // This function refreshes this pointer
    refreshCurrentEnvironmentMap(getCurrentHDRFile().toStdString());
}

void ViewportSettings::presentEnvironmentMaps(core::Vector<model::EnvironmentMapBinding>&& environmentMaps)
{
    m_environmentMapBindings = std::move(environmentMaps);
    QList<QString> hdrFileList;
    hdrFileList.append("Default");

    std::optional<QString> matchedCurrentHDRFile = std::nullopt;
    for (auto& envMap : m_environmentMapBindings) {
        const QString name = envMap.getName().c_str();
        hdrFileList.append(name);

        if (name == m_currentHDRFile) {
            matchedCurrentHDRFile = name;
        }
    }

    emit setupHDRComboBox(hdrFileList);

    setCurrentHDRFile(matchedCurrentHDRFile.value_or("Default"));
}

void ViewportSettings::presentIblSamplerTaskIsDone(const bool isDone)
{
    if (isDone == false) {
        return;
    }

    // this check is necessary since timer.stop also emits the timeout signal -> this method is run twice back to back
    if (m_hdrLoadTimer->isActive() == false) {
        return;
    }

    m_hdrLoadTimer->stop();

    m_environmentMaps.refresh();
}

void ViewportSettings::presentTaskCreated(model::IblSamplerTaskBinding&& _taskBinding)
{
    m_iblSamplerTaskBinding = std::move(_taskBinding);
    m_iblSamplerTaskBinding.bind(this);
    model::IblSamplerTaskEventManager::getInstance()->registerModel(m_iblSamplerTaskBinding);
}

void ViewportSettings::presentGltfOpen(gltf::Gltf*)
{
    // this data is required by the renderer to function after a glTF has been loaded
    setEnvironmentLighting(m_environmentLighting);
    RenderSettingInteractor::getInstance()->setPunctualLightsEnable(m_punctualLights);

    model::ProjectManager::getInstance()->setSelectedMaterialVariant(std::nullopt);
    m_pMaterialVariantsList.refresh();
    RenderSettingInteractor::getInstance()->setToneMapping(m_toneMapping);
    RenderSettingInteractor::getInstance()->setExposure(m_exposure);

    RenderSettingInteractor::getInstance()->setPbrSpecularGlossinessEnabled(m_pbrSpecularGlossiness);
    RenderSettingInteractor::getInstance()->setUnlitEnabled(m_unlit);
    RenderSettingInteractor::getInstance()->setClearCoatEnabled(m_clearCoat);
    RenderSettingInteractor::getInstance()->setSheenEnabled(m_sheen);
    RenderSettingInteractor::getInstance()->setTransmissionEnabled(m_transmission);
    RenderSettingInteractor::getInstance()->setVolumeEnabled(m_volume);
    RenderSettingInteractor::getInstance()->setIridescenceEnabled(m_iridescence);
    RenderSettingInteractor::getInstance()->setIOREnabled(m_ior);
    RenderSettingInteractor::getInstance()->setSpecularEnabled(m_specular);
}

void ViewportSettings::presentGltfImport(gltf::Gltf*)
{
    RenderSettingInteractor::getInstance()->setIBLEnabled(m_environmentLighting);
    RenderSettingInteractor::getInstance()->setPunctualLightsEnable(m_punctualLights);
    m_pMaterialVariantsList.refresh();
}

void ViewportSettings::presentMaterialVariantsSelected(int id, core::Vector<const core::string> materialVariantNames)
{
    QList<QString> variantsList;
    variantsList.append("None");
    for (const core::string& variant : materialVariantNames) {
        variantsList.append(QString(variant.c_str()));
    }
    // use id + 1 to account for the "None" value
    emit materialVariantsChanged(id + 1, variantsList);
}

// Environment

void ViewportSettings::setShowBackground(bool showBackground)
{
    if (m_showBackground == showBackground) {
        return;
    }

    m_showBackground = showBackground;
    RenderSettingInteractor::getInstance()->setCubeMapEnable(showBackground);

    emit showBackgroundChanged();
}

void ViewportSettings::setBackgroundColor(QColor color)
{
    if (m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    Preferences::getInstance()->setClearColor(color);

    emit backgroundColorChanged();
}

void ViewportSettings::openBackgroundColorDialog()
{
    QColor newColor = utils::pickColor([&](const QColor& liveColor) {
        m_backgroundColor = liveColor;
        Preferences::getInstance()->setClearColor(liveColor);

        emit backgroundColorChanged();
    },
        m_backgroundColor, "Background Color", false);

    m_backgroundColor = newColor;
    Preferences::getInstance()->setClearColor(newColor);

    emit backgroundColorChanged();
}

void ViewportSettings::setBlur(int blur)
{
    if (m_blur == blur) {
        return;
    }

    m_blur = blur;
    m_currentEnvironmentMap->setEnvironmentBlurNormalized(static_cast<float>(blur / 100.0f));

    emit blurChanged();
}

void ViewportSettings::setEnvironmentLightingIntensity(float intensity)
{
    if (m_environmentLightingIntensity == intensity) {
        return;
    }

    m_environmentLightingIntensity = intensity;
    m_currentEnvironmentMap->setEnvironmentMapIntensity(intensity);

    emit environmentLightingIntensityChanged();
}

void ViewportSettings::setCurrentHDRFile(QString newHDRFile)
{
    if (m_currentHDRFile == newHDRFile) {
        return;
    }
    refreshCurrentEnvironmentMap(newHDRFile.toStdString());

    setFrontFace(convertAngleToFace(m_currentEnvironmentMap->getEnvironmentMapRotation()));
    setEnvironmentLightingIntensity(m_currentEnvironmentMap->getEnvironmentMapIntensity());
    setBlur(static_cast<int>(m_currentEnvironmentMap->getEnvironmentBlurNormalized() * 100));

    emit environmentMapSelected(*m_currentEnvironmentMap);
    emit currentHDRFileChanged();
}

void ViewportSettings::refreshCurrentEnvironmentMap(const std::string& newHDRFile)
{
    // try to locate the new HDR file in the list of available HDR files
    auto it = std::find_if(m_environmentMapBindings.begin(), m_environmentMapBindings.end(), [&](auto envmap) {
        return newHDRFile == envmap.getName();
    });

    if (it == m_environmentMapBindings.end()) {
        m_currentEnvironmentMap = &m_defaultEnvironmentMap;
        m_currentHDRFile = "Default";
    } else {

        m_currentEnvironmentMap = &*it;
        m_currentHDRFile = QString(it->getName().c_str());
    }
}

void ViewportSettings::addNewHDRFile()
{
    QFileDialog dialog(nullptr, tr("Select HDR File"), "", "hdr (*.hdr)");
    dialog.setAcceptMode(QFileDialog::AcceptOpen);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().empty()) {
        return;
    }

    QString selectedFile = dialog.selectedFiles().front();
    QString selectedFileInfo = "HDR import: loading " + selectedFile;
    core::logInfo(selectedFileInfo.toStdString().c_str());

    auto selectedFilePath = filesystem::Path(m_environmentMaps.getPath().getFilesystem(), selectedFile.toStdString());
#ifdef UX3D_IS_APPLE
    filesystem::IFileSystem::sandboxRequestAccess(selectedFilePath);
#endif

    m_iblSamplerTaskCollectionBinding.create(selectedFilePath, m_environmentMaps.getPath());
    m_iblSamplerTaskCollectionBinding.refresh();
    m_hdrLoadTimer->start(500);
}

void ViewportSettings::deleteCurrentHDRFile()
{
    if (m_currentEnvironmentMap == nullptr || m_currentEnvironmentMap == &m_defaultEnvironmentMap) {
        return;
    }

    m_environmentMaps.deleteEnvironmentMap(*m_currentEnvironmentMap);
}

void ViewportSettings::setFrontFace(QString frontFace)
{
    if (m_frontFace == frontFace) {
        return;
    }

    m_frontFace = frontFace;

    const uint32_t degrees = convertFaceToAngle(frontFace);
    if (m_currentEnvironmentMap != nullptr) {
        m_currentEnvironmentMap->setEnvironmentMapRotation(degrees);
    }

    emit frontFaceChanged();
}

// Lights

void ViewportSettings::setEnvironmentLighting(bool environmentLighting)
{
    if (m_environmentLighting == environmentLighting) {
        return;
    }

    m_environmentLighting = environmentLighting;
    RenderSettingInteractor::getInstance()->setIBLEnabled(environmentLighting);

    emit environmentLightingChanged();
}

void ViewportSettings::setPunctualLights(bool state)
{
    if (m_punctualLights == state) {
        return;
    }

    m_punctualLights = state;
    RenderSettingInteractor::getInstance()->setPunctualLightsEnable(state);

    emit punctualLightsChanged();
}

// Tonemapping

QString ViewportSettings::getToneMapping() const
{
    return QString::fromStdString(renderer::toneMappingToString(m_toneMapping));
}

void ViewportSettings::setToneMapping(const QString toneMapping)
{
    renderer::ToneMappingsEnum newToneMapping = renderer::toneMappingFromString(toneMapping.toStdString());
    if (m_toneMapping == newToneMapping) {
        return;
    }

    m_toneMapping = newToneMapping;
    RenderSettingInteractor::getInstance()->setToneMapping(newToneMapping);
}

void ViewportSettings::setExposure(float exposure)
{
    if (m_exposure == exposure) {
        return;
    }

    m_exposure = exposure;
    RenderSettingInteractor::getInstance()->setExposure(exposure);

    emit exposureChanged();
}

// Extensions

void ViewportSettings::setPbrSpecularGlossiness(bool isEnabled)
{
    if (m_pbrSpecularGlossiness == isEnabled) {
        return;
    }

    m_pbrSpecularGlossiness = isEnabled;
    RenderSettingInteractor::getInstance()->setPbrSpecularGlossinessEnabled(isEnabled);

    emit pbrSpecularGlossinessChanged();
}

void ViewportSettings::setUnlit(bool isEnabled)
{
    if (m_unlit == isEnabled) {
        return;
    }

    m_unlit = isEnabled;
    RenderSettingInteractor::getInstance()->setUnlitEnabled(isEnabled);

    emit unlitChanged();
}

void ViewportSettings::setClearCoat(bool isEnabled)
{
    if (m_clearCoat == isEnabled) {
        return;
    }

    m_clearCoat = isEnabled;
    RenderSettingInteractor::getInstance()->setClearCoatEnabled(isEnabled);

    emit clearCoatChanged();
}

void ViewportSettings::setSheen(bool isEnabled)
{
    if (m_sheen == isEnabled) {
        return;
    }

    m_sheen = isEnabled;
    RenderSettingInteractor::getInstance()->setSheenEnabled(isEnabled);

    emit sheenChanged();
}

void ViewportSettings::setTransmission(bool isEnabled)
{
    if (m_transmission == isEnabled) {
        return;
    }

    // volume extension depends on transmission and can not be viewed without it
    if (isEnabled == false) {
        m_volumePrevState = m_volume;
        if (m_volume) {
            setVolume(false);
        }
    } else if (m_volumePrevState != m_volume) {
        setVolume(m_volumePrevState);
    }

    m_transmission = isEnabled;
    RenderSettingInteractor::getInstance()->setTransmissionEnabled(isEnabled);

    emit transmissionChanged();
}

void ViewportSettings::setVolume(bool isEnabled)
{
    if (m_volume == isEnabled) {
        return;
    }

    m_volume = isEnabled;
    RenderSettingInteractor::getInstance()->setVolumeEnabled(isEnabled);

    emit volumeChanged();
}
void ViewportSettings::setIridescence(bool isEnabled)
{
    if (m_iridescence == isEnabled) {
        return;
    }

    m_iridescence = isEnabled;
    RenderSettingInteractor::getInstance()->setIridescenceEnabled(isEnabled);

    emit iridescenceChanged();
}

void ViewportSettings::setIor(bool isEnabled)
{
    if (m_ior == isEnabled) {
        return;
    }

    m_ior = isEnabled;
    RenderSettingInteractor::getInstance()->setIOREnabled(isEnabled);

    emit iorChanged();
}

void ViewportSettings::setSpecular(bool isEnabled)
{
    if (m_specular == isEnabled) {
        return;
    }

    m_specular = isEnabled;
    RenderSettingInteractor::getInstance()->setSpecularEnabled(isEnabled);

    emit specularChanged();
}

void ViewportSettings::setEmissiveStrength(bool isEnabled)
{
    if (m_emissiveStrength == isEnabled) {
        return;
    }

    m_emissiveStrength = isEnabled;
    RenderSettingInteractor::getInstance()->setEmissiveStrengthEnabled(isEnabled);

    emit emissiveStrengthChanged();
}

// Variants

Q_INVOKABLE void ViewportSettings::setMaterialVariants(int index)
{
    // index - 1 is used to account for the "None" value
    --index;

    auto& list = model::ProjectManager::getInstance()->getGltf().getMaterialVariants();
    gltf::MaterialVariant* materialVariant = nullptr;
    std::optional<uint32_t> materialVariantID = std::nullopt;
    if (index < list.size() || index >= 0) {
        materialVariant = &list.at(index);
        if (materialVariant != nullptr) {
            materialVariantID = materialVariant->m_ID;
        }
    }

    if (materialVariantID == model::ProjectManager::getInstance()->getSelectedMaterialVariant()) {
        return;
    }

    model::ProjectManager::getInstance()->setSelectedMaterialVariant(materialVariantID);
    auto mainWindow = MainWindow::getMainWindow();
    if (mainWindow && mainWindow->m_pRenderer) {
        mainWindow->m_pRenderer->presentMaterialVariantsSelected(materialVariantID);
    }
}

// helpers

const QString ViewportSettings::convertAngleToFace(uint32_t _angle)
{
    switch (_angle % 360) {
    case 0:
        return "+X";
    case 90:
        return "+Z";
    case 180:
        return "-X";
    case 270:
        return "-Z";
    default:
        return "+X";
    }
}

uint32_t ViewportSettings::convertFaceToAngle(const QString& _face)
{
    if (_face == "+X") {
        return 0;
    }
    if (_face == "-X") {
        return 180;
    }
    if (_face == "+Z") {
        return 90;
    }
    if (_face == "-Z") {
        return 270;
    }
    return 0;
}
