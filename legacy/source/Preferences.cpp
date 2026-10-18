#include "Preferences.h"
#include "core/Platform.h"
#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "helpers/QtFileSystem.h"
#include "helpers/Utils.h"

#include <QFileDialog>
#include <QList>
#include <QSettings>
#include <QStandardPaths>

using namespace ux3d;
using namespace ux3d::editor;

Preferences* Preferences::getInstance()
{
    static Preferences instance;
    return &instance;
}

Preferences::Preferences()
{
    initializePreferencesStorage();
}

void Preferences::initializePreferencesStorage()
{
    m_storage.registerEntry(&m_wireframeColor, "WireframeColor");
    m_storage.load(m_wireframeColor);

    m_storage.registerEntry(&m_boundingBoxColor, "BoundingBoxColor");
    m_storage.load(m_boundingBoxColor);

    m_storage.registerEntry(&m_skeletonColor, "SkeletonColor");
    m_storage.load(m_skeletonColor);

    m_storage.registerEntry(&m_gridColor, "GridColor");
    m_storage.load(m_gridColor);

    m_storage.registerEntry(&m_selectionColor, "SelectionColor");
    m_storage.load(m_selectionColor);

    m_storage.registerEntry(&m_outlineColor, "OutlineColor");
    m_storage.load(m_outlineColor);

    m_storage.registerEntry(&m_clearColor, "ClearColor");
    m_storage.load(m_clearColor);

    m_storage.registerEntry(&m_outlineThickness, "OutlineThickness");
    m_storage.load(m_outlineThickness);

    m_storage.registerEntry(&m_selectionColorDisabled, "SelectionColorDisabled");
    m_storage.load(m_selectionColorDisabled);

    m_storage.registerEntry(&m_loadSampleAssetOnStartup, "LoadSampleAssetOnStartup");
    m_storage.load(m_loadSampleAssetOnStartup);

    m_storage.registerEntry(&m_freeCameraSpeedScale, "FreeCameraSpeedScale");
    m_storage.load(m_freeCameraSpeedScale);

    m_storage.registerEntry(&m_cameraZoomSpeedScale, "CameraZoomSpeedScale");
    m_storage.load(m_cameraZoomSpeedScale);

    m_storage.registerEntry(&m_cameraPanSpeedScale, "CameraPanSpeedScale");
    m_storage.load(m_cameraPanSpeedScale);

    m_storage.registerEntry(&m_gizmoScaling, "GizmoScaling");
    m_storage.load(m_gizmoScaling);

    m_storage.registerEntry(&m_showWelcomeOnStartup, "ShowWelcomeOnStartup");
    m_storage.load(m_showWelcomeOnStartup);

    m_storage.registerEntry(&m_maxRecentFiles, "MaxRecentFiles");
    m_storage.load(m_maxRecentFiles);

    m_storage.registerEntry(&m_environmentsVersion, "EnvironmentsVersion");
    m_storage.load(m_environmentsVersion);

    m_storage.registerEntry(&m_msaa, "MSAAExponent");
    m_storage.load(m_msaa);

    m_storage.registerEntry(&m_iblMaxResolutionExp, "IBLMaxResolutionExp");
    m_storage.load(m_iblMaxResolutionExp);

    m_storage.registerEntry(&m_overwriteResourceFilesWithoutAsking, "OverwriteResourceFiles");
    m_storage.load(m_overwriteResourceFilesWithoutAsking);

    m_storage.registerEntry(&m_validateAfterSave, "ValidateAfterSave");
    m_storage.load(m_validateAfterSave);

    m_storage.registerEntry(&m_dracoMeshCompressionLevel, "DracoMeshCompressionLevel");
    m_storage.load(m_dracoMeshCompressionLevel);

    m_storage.registerEntry(&m_dracoMeshCompressionPreserveOrder, "DracoMeshCompressionPreserveOrder");
    m_storage.load(m_dracoMeshCompressionPreserveOrder);

    m_storage.registerEntry(&m_dracoMeshCompressionOptimizeIndices, "DracoMeshCompressionOptimizeIndices");
    m_storage.load(m_dracoMeshCompressionOptimizeIndices);

    m_storage.registerEntry(&m_dracoMeshCompressionQuantizationBits.position, "DracoMeshCompressionQuantizationBitsPosition");
    m_storage.load(m_dracoMeshCompressionQuantizationBits.position);
    m_storage.registerEntry(&m_dracoMeshCompressionQuantizationBits.normal, "DracoMeshCompressionQuantizationBitsNormal");
    m_storage.load(m_dracoMeshCompressionQuantizationBits.normal);
    m_storage.registerEntry(&m_dracoMeshCompressionQuantizationBits.uv, "DracoMeshCompressionQuantizationBitsUV");
    m_storage.load(m_dracoMeshCompressionQuantizationBits.uv);
    m_storage.registerEntry(&m_dracoMeshCompressionQuantizationBits.color, "DracoMeshCompressionQuantizationBitsColor");
    m_storage.load(m_dracoMeshCompressionQuantizationBits.color);
    m_storage.registerEntry(&m_dracoMeshCompressionQuantizationBits.generic, "DracoMeshCompressionQuantizationBitsGeneric");
    m_storage.load(m_dracoMeshCompressionQuantizationBits.generic);

    m_storage.registerEntry(&m_imageEditingToolPath, "ImageEditingToolPath");
    m_storage.load(m_imageEditingToolPath);
}

const QString& Preferences::getInitialFilename() const
{
    return m_initialFilename;
}

void Preferences::setInitialFilename(const QString& _initialFilename)
{
    auto filename = _initialFilename;
    filename.replace("\\", "/");

    if (m_initialFilename == filename)
        return; // nothing changed

    m_initialFilename = filename;
    emit initialFilenameChanged();
}

const QString& Preferences::getEnvironmentDirectory() const
{
    return m_environmentDirectory;
}

void Preferences::setEnvironmentDirectory(const QString& _environmentDirectory)
{
    auto filename = _environmentDirectory;
    filename.replace("\\", "/");

    if (m_environmentDirectory == filename)
        return; // nothing changed

    m_environmentDirectory = filename;
    emit environmentDirectoryChanged();
}

const QString& Preferences::getImageEditingToolPath()
{
    m_storage.load(m_imageEditingToolPath);
    return m_imageEditingToolPath;
}

void Preferences::setImageEditingToolPath(const QString& _imageEditingToolPath)
{
    QString filename = _imageEditingToolPath;
    filename.replace("\\", "/");

    filesystem::Path path = filesystem::Path(QtFileSystem::instance(), filename.toStdString());
    if (!filename.isEmpty() && !path.exists()) {
        emit imageEditingToolPathChanged();
        return;
    }

    if (m_imageEditingToolPath == filename) {
        return; // nothing changed
    }

    m_imageEditingToolPath = filename;
    m_storage.store(m_imageEditingToolPath);
    emit imageEditingToolPathChanged();
}

const QColor Preferences::getSelectionColor(bool _forSettings)
{
    m_storage.load(m_selectionColor);
    m_storage.load(m_selectionColorDisabled);
    if (m_selectionColorDisabled && _forSettings == false) {
        QColor copy = m_selectionColor;
        copy.setAlphaF(0.0f);
        return copy;
    }
    return m_selectionColor;
}

void Preferences::setSelectionColor(const QColor& _selectionColor)
{
    if (m_selectionColor == _selectionColor)
        return; // nothing changed

    m_selectionColor = _selectionColor;
    m_storage.store(m_selectionColor);
    emit selectionColorChanged();
}

const QColor& Preferences::getOutlineColor()
{
    m_storage.load(m_outlineColor);
    return m_outlineColor;
}

void Preferences::setOutlineColor(const QColor& _outlineColor)
{
    if (m_outlineColor == _outlineColor)
        return; // nothing changed

    m_outlineColor = _outlineColor;
    m_storage.store(m_outlineColor);
    emit outlineColorChanged();
}

void Preferences::openOutlineColorDialog()
{
    QColor newColor = utils::pickColor([&](const QColor& liveColor) {
        m_outlineColor = liveColor;
        m_storage.store(m_outlineColor);

        emit outlineColorChanged();
    },
        m_outlineColor, "Outline Color", false);

    m_outlineColor = newColor;
    m_storage.store(m_outlineColor);

    emit outlineColorChanged();
}

const QColor& Preferences::getClearColor()
{
    m_storage.load(m_clearColor);
    return m_clearColor;
}

void Preferences::setClearColor(const QColor& _clearColor)
{
    if (m_clearColor == _clearColor)
        return; // nothing changed

    m_clearColor = _clearColor;
    m_storage.store(m_clearColor);
}

bool Preferences::getSelectionColorDisabled()
{
    m_storage.load(m_selectionColorDisabled);
    return m_selectionColorDisabled;
}

void Preferences::setSelectionColorDisabled(bool _disabled)
{
    if (m_selectionColorDisabled == _disabled)
        return; // nothing changed

    m_selectionColorDisabled = _disabled;
    m_storage.store(m_selectionColorDisabled);
    emit selectionColorDisabledChanged();
}

QColor Preferences::getWireframeColor()
{
    m_storage.load(m_wireframeColor);
    return m_wireframeColor;
}

void Preferences::setWireframeColor(const QColor& _wireframeColor)
{
    if (m_wireframeColor == _wireframeColor)
        return; // nothing changed

    m_wireframeColor = _wireframeColor;
    m_storage.store(m_wireframeColor);
    emit wireframeColorChanged();
}

void Preferences::openWireframeColorDialog()
{
    QColor newColor = utils::pickColor([&](const QColor& liveColor) {
        m_wireframeColor = liveColor;
        m_storage.store(m_wireframeColor);

        emit wireframeColorChanged();
    },
        m_wireframeColor, "Wireframe Color", false);

    m_wireframeColor = newColor;
    m_storage.store(m_wireframeColor);

    emit wireframeColorChanged();
}

QColor Preferences::getGridColor()
{
    m_storage.load(m_gridColor);
    return m_gridColor;
}

void Preferences::setGridColor(const QColor& _gridColor)
{
    if (m_gridColor == _gridColor)
        return; // nothing changed

    m_gridColor = _gridColor;
    m_storage.store(m_gridColor);
    emit gridColorChanged();
}

void Preferences::openGridColorDialog()
{
    QColor newColor = utils::pickColor([&](const QColor& liveColor) {
        m_gridColor = liveColor;
        m_storage.store(m_gridColor);

        emit gridColorChanged();
    },
        m_gridColor, "Grid Color", false);

    m_gridColor = newColor;
    m_storage.store(m_gridColor);

    emit gridColorChanged();
}

QColor Preferences::getBoundingBoxColor()
{
    m_storage.load(m_boundingBoxColor);
    return m_boundingBoxColor;
}

void Preferences::setBoundingBoxColor(const QColor& color)
{
    if (m_boundingBoxColor == color)
        return; // nothing changed

    m_boundingBoxColor = color;
    m_storage.store(m_boundingBoxColor);
    emit boundingBoxColorChanged();
}

void Preferences::openBoundingBoxColorDialog()
{
    QColor newColor = utils::pickColor([&](const QColor& liveColor) {
        m_boundingBoxColor = liveColor;
        m_storage.store(m_boundingBoxColor);

        emit boundingBoxColorChanged();
    },
        m_boundingBoxColor, "Bounding Box Color", false);

    m_boundingBoxColor = newColor;
    m_storage.store(m_boundingBoxColor);

    emit boundingBoxColorChanged();
}

QColor Preferences::getSkeletonColor()
{
    m_storage.load(m_skeletonColor);
    return m_skeletonColor;
}

void Preferences::setSkeletonColor(const QColor& color)
{
    if (m_skeletonColor == color)
        return; // nothing changed

    m_skeletonColor = color;
    m_storage.store(m_skeletonColor);
    emit skeletonColorChanged();
}

void Preferences::openSkeletonColorDialog()
{
    QColor newColor = utils::pickColor([&](const QColor& liveColor) {
        m_skeletonColor = liveColor;
        m_storage.store(m_skeletonColor);

        emit skeletonColorChanged();
    },
        m_skeletonColor, "Skeleton Color", false);

    m_skeletonColor = newColor;
    m_storage.store(m_skeletonColor);

    emit skeletonColorChanged();
}

int Preferences::getOutlineThickness()
{
    m_storage.load(m_outlineThickness);
    return m_outlineThickness;
}

void Preferences::setOutlineThickness(int _thickness)
{
    if (m_outlineThickness == _thickness)
        return; // nothing changed

    m_outlineThickness = _thickness;
    m_storage.store(m_outlineThickness);
    emit outlineThicknessChanged();
}

bool Preferences::getLoadSampleAssetOnStartup()
{
    m_storage.load(m_loadSampleAssetOnStartup);
    return m_loadSampleAssetOnStartup;
}

void Preferences::setLoadSampleAssetOnStartup(bool _enabled)
{
    if (m_loadSampleAssetOnStartup == _enabled)
        return; // nothing changed

    m_loadSampleAssetOnStartup = _enabled;
    m_storage.store(m_loadSampleAssetOnStartup);
    emit loadSampleAssetOnStartupChanged();
}

int Preferences::getMaxRecentFiles()
{
    m_storage.load(m_maxRecentFiles);
    return m_maxRecentFiles;
}

void Preferences::setMaxRecentFiles(int _fileCount)
{
    if (m_maxRecentFiles == _fileCount)
        return; // nothing changed

    m_maxRecentFiles = _fileCount;
    m_storage.store(m_maxRecentFiles);
    emit maxRecentFilesChanged();
}

bool Preferences::getOverwriteResourceFilesWithoutAsking()
{
    m_storage.load(m_overwriteResourceFilesWithoutAsking);
    return m_overwriteResourceFilesWithoutAsking;
}

void Preferences::setOverwriteResourceFilesWithoutAsking(bool _enabled)
{
    if (m_overwriteResourceFilesWithoutAsking == _enabled)
        return; // nothing changed

    m_overwriteResourceFilesWithoutAsking = _enabled;
    m_storage.store(m_overwriteResourceFilesWithoutAsking);
    emit overwriteResourceFilesWithoutAskingChanged();
}

bool Preferences::getValidateAfterSave()
{
    m_storage.load(m_validateAfterSave);
    return m_validateAfterSave;
}

void Preferences::setValidateAfterSave(bool _enabled)
{
    if (m_validateAfterSave == _enabled) {
        return; // nothing changed
    }
    m_validateAfterSave = _enabled;
    m_storage.store(m_validateAfterSave);
    emit validateAfterSaveChanged();
}

void ux3d::editor::Preferences::setDracoMeshCompressionPreserveOrder(bool _enabled)
{
    if (m_dracoMeshCompressionPreserveOrder == _enabled)
        return; // nothing changed

    m_dracoMeshCompressionPreserveOrder = _enabled;
    m_storage.store(m_dracoMeshCompressionPreserveOrder);
    emit dracoMeshCompressionPreserveOrderChanged();
}

bool ux3d::editor::Preferences::getDracoMeshCompressionPreserveOrder()
{
    m_storage.load(m_dracoMeshCompressionPreserveOrder);
    return m_dracoMeshCompressionPreserveOrder;
}

void ux3d::editor::Preferences::setDracoMeshCompressionOptimizeIndices(bool _enabled)
{
    if (m_dracoMeshCompressionOptimizeIndices == _enabled)
        return; // nothing changed

    m_dracoMeshCompressionOptimizeIndices = _enabled;
    m_storage.store(m_dracoMeshCompressionOptimizeIndices);
    emit dracoMeshCompressionOptimizeIndicesChanged();
}

bool ux3d::editor::Preferences::getDracoMeshCompressionOptimizeIndices()
{
    m_storage.load(m_dracoMeshCompressionOptimizeIndices);
    return m_dracoMeshCompressionOptimizeIndices;
}

void Preferences::setDracoMeshCompressionLevel(int32_t _level)
{
    if (m_dracoMeshCompressionLevel == _level)
        return; // nothing changed

    m_dracoMeshCompressionLevel = _level;
    m_storage.store(m_dracoMeshCompressionLevel);
    emit dracoMeshCompressionLevelChanged();
}

int32_t ux3d::editor::Preferences::getDracoMeshCompressionLevel()
{
    m_storage.load(m_dracoMeshCompressionLevel);
    return m_dracoMeshCompressionLevel;
}

const gltf::DracoEncoder::QuantizationBits& Preferences::getDracoMeshCompressionQuantizationBits()
{
    static_assert(sizeof(m_dracoMeshCompressionQuantizationBits) == sizeof(float[5]), "Number of quantization bits options must be statically known here.");
    m_storage.load(m_dracoMeshCompressionQuantizationBits.position);
    m_storage.load(m_dracoMeshCompressionQuantizationBits.normal);
    m_storage.load(m_dracoMeshCompressionQuantizationBits.uv);
    m_storage.load(m_dracoMeshCompressionQuantizationBits.color);
    m_storage.load(m_dracoMeshCompressionQuantizationBits.generic);
    return m_dracoMeshCompressionQuantizationBits;
}

int Preferences::getDracoMeshCompressionPositionQuantizationBits()
{
    m_storage.load(m_dracoMeshCompressionQuantizationBits.position);
    return m_dracoMeshCompressionQuantizationBits.position;
}

int Preferences::getDracoMeshCompressionNormalQuantizationBits()
{
    m_storage.load(m_dracoMeshCompressionQuantizationBits.normal);
    return m_dracoMeshCompressionQuantizationBits.normal;
}

int Preferences::getDracoMeshCompressionUVQuantizationBits()
{
    m_storage.load(m_dracoMeshCompressionQuantizationBits.uv);
    return m_dracoMeshCompressionQuantizationBits.uv;
}

int Preferences::getDracoMeshCompressionColorQuantizationBits()
{
    m_storage.load(m_dracoMeshCompressionQuantizationBits.color);
    return m_dracoMeshCompressionQuantizationBits.color;
}

int Preferences::getDracoMeshCompressionGenericQuantizationBits()
{
    m_storage.load(m_dracoMeshCompressionQuantizationBits.generic);
    return m_dracoMeshCompressionQuantizationBits.generic;
}

void Preferences::setDracoMeshCompressionPositionQuantizationBits(int32_t _bits)
{
    if (m_dracoMeshCompressionQuantizationBits.position == _bits)
        return; // nothing changed

    m_dracoMeshCompressionQuantizationBits.position = _bits;
    m_storage.store(m_dracoMeshCompressionQuantizationBits.position);
    emit dracoMeshCompressionPositionQuantizationBitsChanged();
}

void Preferences::setDracoMeshCompressionNormalQuantizationBits(int32_t _bits)
{
    if (m_dracoMeshCompressionQuantizationBits.normal == _bits)
        return; // nothing changed

    m_dracoMeshCompressionQuantizationBits.normal = _bits;
    m_storage.store(m_dracoMeshCompressionQuantizationBits.normal);
    emit dracoMeshCompressionNormalQuantizationBitsChanged();
}

void Preferences::setDracoMeshCompressionUVQuantizationBits(int32_t _bits)
{
    if (m_dracoMeshCompressionQuantizationBits.uv == _bits)
        return; // nothing changed

    m_dracoMeshCompressionQuantizationBits.uv = _bits;
    m_storage.store(m_dracoMeshCompressionQuantizationBits.uv);
    emit dracoMeshCompressionUVQuantizationBitsChanged();
}

void Preferences::setDracoMeshCompressionColorQuantizationBits(int32_t _bits)
{
    if (m_dracoMeshCompressionQuantizationBits.color == _bits)
        return; // nothing changed

    m_dracoMeshCompressionQuantizationBits.color = _bits;
    m_storage.store(m_dracoMeshCompressionQuantizationBits.color);
    emit dracoMeshCompressionColorQuantizationBitsChanged();
}

void Preferences::setDracoMeshCompressionGenericQuantizationBits(int32_t _bits)
{
    if (m_dracoMeshCompressionQuantizationBits.generic == _bits)
        return; // nothing changed

    m_dracoMeshCompressionQuantizationBits.generic = _bits;
    m_storage.store(m_dracoMeshCompressionQuantizationBits.generic);
    emit dracoMeshCompressionGenericQuantizationBitsChanged();
}

DefaultCameraPreferences Preferences::getDefaultCameraPreferences() const
{
    return DefaultCameraPreferences();
}

double Preferences::getFreeCameraSpeedScale()
{
    m_storage.load(m_freeCameraSpeedScale);
    return m_freeCameraSpeedScale;
}

void Preferences::setFreeCameraSpeedScale(double _value)
{
    if (m_freeCameraSpeedScale == _value)
        return; // nothing changed

    m_freeCameraSpeedScale = _value;
    m_storage.store(m_freeCameraSpeedScale);
    emit freeCameraSpeedScaleChanged();
}

double Preferences::getCameraPanSpeedScale()
{
    m_storage.load(m_cameraPanSpeedScale);
    return m_cameraPanSpeedScale;
}

void Preferences::setCameraPanSpeedScale(double _value)
{
    if (m_cameraPanSpeedScale == _value)
        return; // nothing changed

    m_cameraPanSpeedScale = _value;
    m_storage.store(m_cameraPanSpeedScale);
    emit cameraPanSpeedScaleChanged();
}

double Preferences::getCameraZoomSpeedScale()
{
    m_storage.load(m_cameraZoomSpeedScale);
    return m_cameraZoomSpeedScale;
}

void Preferences::setCameraZoomSpeedScale(double _value)
{
    if (m_cameraZoomSpeedScale == _value)
        return; // nothing changed

    m_cameraZoomSpeedScale = _value;
    m_storage.store(m_cameraZoomSpeedScale);
    emit cameraZoomSpeedScaleChanged();
}

NavigationSpeedPreferences Preferences::getNavigationSpeedPreferences() const
{
    return NavigationSpeedPreferences();
}

double Preferences::getGizmoScaling()
{
    m_storage.load(m_gizmoScaling);
    return m_gizmoScaling;
}

void Preferences::setGizmoScaling(double _value)
{
    if (m_gizmoScaling == _value)
        return; // nothing changed

    m_gizmoScaling = _value;
    m_storage.store(m_gizmoScaling);
    emit gizmoScalingChanged();
}

void Preferences::setShowWelcomeOnStartup(bool _enabled)
{
    if (m_showWelcomeOnStartup == _enabled)
        return; // nothing changed

    m_showWelcomeOnStartup = _enabled;
    m_storage.store(m_showWelcomeOnStartup);
}

bool Preferences::getShowWelcomeOnStartup()
{
    m_storage.load(m_showWelcomeOnStartup);
    return m_showWelcomeOnStartup;
}
int32_t Preferences::getMSAAExp()
{
    m_storage.load(m_msaa);
    return m_msaa;
}

void Preferences::setMSAAExp(int32_t msaa)
{
    if (m_msaa == msaa)
        return; // nothing changed

    m_msaa = msaa;
    m_storage.store(m_msaa);
    emit msaaExpChanged(msaa);
}

void Preferences::setIblMaxResolutionExp(int value)
{
    if (m_iblMaxResolutionExp == value)
        return;

    m_iblMaxResolutionExp = value;
    m_storage.store(m_iblMaxResolutionExp);
    emit iblMaxResolutionExpChanged();
}

int Preferences::getIblMaxResolutionExp()
{
    m_storage.load(m_iblMaxResolutionExp);
    return m_iblMaxResolutionExp;
}

int Preferences::getEnvironmentsVersion()
{
    m_storage.load(m_environmentsVersion);
    return m_environmentsVersion;
}

void Preferences::setEnvironmentsVersion(int _version)
{
    m_environmentsVersion = _version;
    m_storage.store(m_environmentsVersion);
}

void Preferences::chooseImageEditingTool()
{
    QString applicationLocation = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation).first();
#ifdef UX3D_IS_APPLE
    applicationLocation = "/Applications";
#endif
#ifdef UX3D_IS_LINUX
    applicationLocation = "/usr/bin";
#endif

    QFileDialog dialog(nullptr, tr("Choose image editing software"), applicationLocation);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setLabelText(QFileDialog::Accept, "Choose");

#ifdef UX3D_IS_WINDOWS
    dialog.setNameFilter("exe (*.exe)");
    dialog.setFilter(QDir::Files);
#endif
#ifdef UX3D_IS_APPLE
    dialog.setNameFilter("app (*.app)");
#endif
#ifdef UX3D_IS_LINUX
    // allow all extensions
#endif

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().empty()) {
        return;
    }

    const QString selectedFile = dialog.selectedFiles().front();
    setImageEditingToolPath(selectedFile);
}
