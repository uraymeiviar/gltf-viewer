#pragma once

#include "core/String.h"
#include "gltf/DracoEncoder.h"
#include "helpers/PreferencesStorage.h"
#include "imageformats/imageformats.h"
#include "math/VectorTypes.h"

#include <QColor>
#include <QObject>
#include <QString>

#include <QtQml>
#include <qqml.h>

namespace ux3d::editor {
struct DefaultCameraPreferences {
    // perspective
    double znearPerspective = 0.01;
    double zfarPerspective = 1000;
    double yfov = 1.0;
    double aspectRatio = 16.0 / 9.0;

    // orthographic
    double znearOrthographic = 0.01;
    double zfarOrthographic = 1000;
    double xmag = 1.0;
    double ymag = 1.0;
};

struct NavigationSpeedPreferences {
    // fitUserCameraToScene
    const float zoomSpeedDenominator = 2500.f;
    const float freeCamSpeedDenominator = 2.f;
    const float panSpeedBaseDenomiantor = 1200.f;

    // rotateCamera
    const float rotationSpeed = 1.f / 180.f;
    const float yRotationMax = math::pi<float>() / 2.f - 0.01f;

    // HandleFreeCamera
    const float rotationSpeedFactor_handleFreeCamera = 2;
    const float rotationSpeed_handleFreeCamera = -1.f / (180.f * rotationSpeedFactor_handleFreeCamera);
};

// This class provides default values, getter and setter for preference values
// To store variables, they have to be registered with a unique name.
// Furthermore, each getter and setter has to use load/store functions

class Preferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString initialFileName READ getInitialFilename WRITE setInitialFilename NOTIFY initialFilenameChanged)
    Q_PROPERTY(QString environmentDirectory READ getEnvironmentDirectory WRITE setEnvironmentDirectory NOTIFY environmentDirectoryChanged)
    Q_PROPERTY(QString imageEditingToolPath READ getImageEditingToolPath WRITE setImageEditingToolPath NOTIFY imageEditingToolPathChanged)

    Q_PROPERTY(QColor selectionColor READ getSelectionColor WRITE setSelectionColor NOTIFY selectionColorChanged)
    Q_PROPERTY(QColor outlineColor READ getOutlineColor WRITE setOutlineColor NOTIFY outlineColorChanged)
    Q_PROPERTY(int outlineThickness READ getOutlineThickness WRITE setOutlineThickness NOTIFY outlineThicknessChanged)
    Q_PROPERTY(bool selectionColorDisabled READ getSelectionColorDisabled WRITE setSelectionColorDisabled NOTIFY selectionColorDisabledChanged)

    Q_PROPERTY(QColor wireframeColor READ getWireframeColor WRITE setWireframeColor NOTIFY wireframeColorChanged)
    Q_PROPERTY(QColor gridColor READ getGridColor WRITE setGridColor NOTIFY gridColorChanged)
    Q_PROPERTY(QColor boundingBoxColor READ getBoundingBoxColor WRITE setBoundingBoxColor NOTIFY boundingBoxColorChanged)
    Q_PROPERTY(QColor skeletonColor READ getSkeletonColor WRITE setSkeletonColor NOTIFY skeletonColorChanged)

    Q_PROPERTY(int msaaExp READ getMSAAExp WRITE setMSAAExp NOTIFY msaaExpChanged)
    Q_PROPERTY(int iblMaxResolutionExp READ getIblMaxResolutionExp WRITE setIblMaxResolutionExp NOTIFY iblMaxResolutionExpChanged)

    Q_PROPERTY(bool loadSampleAssetOnStartup READ getLoadSampleAssetOnStartup WRITE setLoadSampleAssetOnStartup NOTIFY loadSampleAssetOnStartupChanged)
    Q_PROPERTY(int maxRecentFiles READ getMaxRecentFiles WRITE setMaxRecentFiles NOTIFY maxRecentFilesChanged)

    Q_PROPERTY(double freeCameraSpeedScale READ getFreeCameraSpeedScale WRITE setFreeCameraSpeedScale NOTIFY freeCameraSpeedScaleChanged)
    Q_PROPERTY(double cameraPanSpeedScale READ getCameraPanSpeedScale WRITE setCameraPanSpeedScale NOTIFY cameraPanSpeedScaleChanged)
    Q_PROPERTY(double cameraZoomSpeedScale READ getCameraZoomSpeedScale WRITE setCameraZoomSpeedScale NOTIFY cameraZoomSpeedScaleChanged)

    Q_PROPERTY(double gizmoScaling READ getGizmoScaling WRITE setGizmoScaling NOTIFY gizmoScalingChanged)

    Q_PROPERTY(bool overwriteResourceFilesWithoutAsking READ getOverwriteResourceFilesWithoutAsking WRITE setOverwriteResourceFilesWithoutAsking NOTIFY overwriteResourceFilesWithoutAskingChanged)
    Q_PROPERTY(bool validateAfterSave READ getValidateAfterSave WRITE setValidateAfterSave NOTIFY validateAfterSaveChanged)

    Q_PROPERTY(bool dracoMeshCompressionPreserveOrder READ getDracoMeshCompressionPreserveOrder WRITE setDracoMeshCompressionPreserveOrder NOTIFY dracoMeshCompressionPreserveOrderChanged)
    Q_PROPERTY(bool dracoMeshCompressionOptimizeIndices READ getDracoMeshCompressionOptimizeIndices WRITE setDracoMeshCompressionOptimizeIndices NOTIFY dracoMeshCompressionOptimizeIndicesChanged)
    Q_PROPERTY(int dracoMeshCompressionLevel READ getDracoMeshCompressionLevel WRITE setDracoMeshCompressionLevel NOTIFY dracoMeshCompressionLevelChanged)
    Q_PROPERTY(int dracoMeshCompressionPositionQuantizationBits READ getDracoMeshCompressionPositionQuantizationBits WRITE setDracoMeshCompressionPositionQuantizationBits NOTIFY dracoMeshCompressionPositionQuantizationBitsChanged)
    Q_PROPERTY(int dracoMeshCompressionNormalQuantizationBits READ getDracoMeshCompressionNormalQuantizationBits WRITE setDracoMeshCompressionNormalQuantizationBits NOTIFY dracoMeshCompressionNormalQuantizationBitsChanged)
    Q_PROPERTY(int dracoMeshCompressionUVQuantizationBits READ getDracoMeshCompressionUVQuantizationBits WRITE setDracoMeshCompressionUVQuantizationBits NOTIFY dracoMeshCompressionUVQuantizationBitsChanged)
    Q_PROPERTY(int dracoMeshCompressionColorQuantizationBits READ getDracoMeshCompressionColorQuantizationBits WRITE setDracoMeshCompressionColorQuantizationBits NOTIFY dracoMeshCompressionColorQuantizationBitsChanged)
    Q_PROPERTY(int dracoMeshCompressionGenericQuantizationBits READ getDracoMeshCompressionGenericQuantizationBits WRITE setDracoMeshCompressionGenericQuantizationBits NOTIFY dracoMeshCompressionGenericQuantizationBitsChanged)

public:
    static Preferences* getInstance();
    Preferences(Preferences const&) = delete;
    void operator=(Preferences const&) = delete;

    const QString& getInitialFilename() const;
    void setInitialFilename(const QString& _initialFilename);

    const QString& getEnvironmentDirectory() const;
    void setEnvironmentDirectory(const QString& _environmentDirectory);

    const QString& getImageEditingToolPath();
    void setImageEditingToolPath(const QString& _imageEditingToolPath);

    const QColor getSelectionColor(bool _forSettings = false);
    void setSelectionColor(const QColor& _selectionColor);
    const QColor& getOutlineColor();
    void setOutlineColor(const QColor& _outlineColor);
    Q_INVOKABLE void openOutlineColorDialog();
    const QColor& getClearColor();
    void setClearColor(const QColor& _clearColor);
    int getOutlineThickness();
    void setOutlineThickness(int _thickness);
    bool getSelectionColorDisabled();
    void setSelectionColorDisabled(bool _disabled);

    QColor getWireframeColor();
    void setWireframeColor(const QColor& _color);
    Q_INVOKABLE void openWireframeColorDialog();

    QColor getGridColor();
    void setGridColor(const QColor& color);
    Q_INVOKABLE void openGridColorDialog();

    QColor getBoundingBoxColor();
    void setBoundingBoxColor(const QColor& _color);
    Q_INVOKABLE void openBoundingBoxColorDialog();

    QColor getSkeletonColor();
    void setSkeletonColor(const QColor& _color);
    Q_INVOKABLE void openSkeletonColorDialog();

    int getMSAAExp();
    void setMSAAExp(int _msaaLevel);
    void setIblMaxResolutionExp(int value);
    int getIblMaxResolutionExp();

    bool getLoadSampleAssetOnStartup();
    void setLoadSampleAssetOnStartup(bool _enabled);

    bool getShowWelcomeOnStartup();
    void setShowWelcomeOnStartup(bool _enabled);

    int getMaxRecentFiles();
    void setMaxRecentFiles(int _fileCount);

    // TODO: remove or add to preferences
    DefaultCameraPreferences getDefaultCameraPreferences() const;

    // TODO: remove or add to preferences
    NavigationSpeedPreferences getNavigationSpeedPreferences() const;

    double getFreeCameraSpeedScale();
    void setFreeCameraSpeedScale(double _value);
    double getCameraPanSpeedScale();
    void setCameraPanSpeedScale(double _value);
    double getCameraZoomSpeedScale();
    void setCameraZoomSpeedScale(double _value);

    double getGizmoScaling();
    void setGizmoScaling(double _value);

    bool getOverwriteResourceFilesWithoutAsking();
    void setOverwriteResourceFilesWithoutAsking(bool _enabled);

    bool getValidateAfterSave();
    void setValidateAfterSave(bool _enabled);

    // TODO: this should also never be exposed to the preferences gui, find a better place for it
    int getEnvironmentsVersion();
    void setEnvironmentsVersion(int _version);

    void setDracoMeshCompressionPreserveOrder(bool _enabled);
    bool getDracoMeshCompressionPreserveOrder();
    void setDracoMeshCompressionOptimizeIndices(bool _enabled);
    bool getDracoMeshCompressionOptimizeIndices();
    void setDracoMeshCompressionLevel(int _level);
    int getDracoMeshCompressionLevel();
    const gltf::DracoEncoder::QuantizationBits& getDracoMeshCompressionQuantizationBits();
    int getDracoMeshCompressionPositionQuantizationBits();
    int getDracoMeshCompressionNormalQuantizationBits();
    int getDracoMeshCompressionUVQuantizationBits();
    int getDracoMeshCompressionColorQuantizationBits();
    int getDracoMeshCompressionGenericQuantizationBits();
    void setDracoMeshCompressionPositionQuantizationBits(int _bits);
    void setDracoMeshCompressionNormalQuantizationBits(int _bits);
    void setDracoMeshCompressionUVQuantizationBits(int _bits);
    void setDracoMeshCompressionColorQuantizationBits(int _bits);
    void setDracoMeshCompressionGenericQuantizationBits(int _bits);

    Q_INVOKABLE void chooseImageEditingTool();

signals:
    void initialFilenameChanged();
    void environmentDirectoryChanged();
    void imageEditingToolPathChanged();
    void selectionColorChanged();
    void outlineColorChanged();
    void outlineThicknessChanged();
    void selectionColorDisabledChanged();
    void wireframeColorChanged();
    void boundingBoxColorChanged();
    void skeletonColorChanged();
    void gridColorChanged();
    void msaaExpChanged(int msaaExponent);
    void iblMaxResolutionExpChanged();
    void loadSampleAssetOnStartupChanged();
    void maxRecentFilesChanged();
    void freeCameraSpeedScaleChanged();
    void cameraPanSpeedScaleChanged();
    void cameraZoomSpeedScaleChanged();
    void gizmoScalingChanged();
    void overwriteResourceFilesWithoutAskingChanged();
    void validateAfterSaveChanged();

    void dracoMeshCompressionPreserveOrderChanged();
    void dracoMeshCompressionOptimizeIndicesChanged();
    void dracoMeshCompressionLevelChanged();
    void dracoMeshCompressionPositionQuantizationBitsChanged();
    void dracoMeshCompressionNormalQuantizationBitsChanged();
    void dracoMeshCompressionUVQuantizationBitsChanged();
    void dracoMeshCompressionColorQuantizationBitsChanged();
    void dracoMeshCompressionGenericQuantizationBitsChanged();

private:
    QString m_initialFilename = "";
    QString m_environmentDirectory = QString(UX3D_RENDERER_RESOURCES_DIR) + "/environments/footprint_court.hdr";
    QString m_imageEditingToolPath = "";

    bool m_showWelcomeOnStartup = true;

    bool m_loadSampleAssetOnStartup = true;
    bool m_validateAfterSave = true;
    bool m_overwriteResourceFilesWithoutAsking = true;

    int32_t m_maxRecentFiles = 10;

    uint32_t m_environmentsVersion = 0;

    int32_t m_msaa = 1;
    int32_t m_iblMaxResolutionExp = 10;

    double m_freeCameraSpeedScale = 1.0;
    double m_cameraPanSpeedScale = 1.0;
    double m_cameraZoomSpeedScale = 1.0;

    double m_gizmoScaling = 1.0;

    QColor m_selectionColor = QColor(185, 219, 97);
    QColor m_outlineColor = QColor(185, 219, 97);
    QColor m_clearColor = QColor(0, 0, 0);
    QColor m_wireframeColor = QColor(255, 255, 255);
    QColor m_boundingBoxColor = QColor(255, 255, 0);
    QColor m_skeletonColor = QColor(0, 255, 255);
    QColor m_gridColor = QColor(184, 184, 184);
    int m_outlineThickness = 5;
    bool m_selectionColorDisabled = true;

    int m_dracoMeshCompressionLevel = 5;
    bool m_dracoMeshCompressionPreserveOrder = false;
    bool m_dracoMeshCompressionOptimizeIndices = false;
    gltf::DracoEncoder::QuantizationBits m_dracoMeshCompressionQuantizationBits;

    Preferences();
    ~Preferences() = default;
    void initializePreferencesStorage();
    PreferencesStorage m_storage;
};
}
