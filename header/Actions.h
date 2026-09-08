// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once

#include <QObject>

#include <tuple>
#include <vector>

#include "enums/EditorRenderLayerEnums.h" // -> enum class RenderMode

class QAction;
class QActionGroup;

namespace ux3d::editor {

class Actions : public QObject {
    Q_OBJECT

public:
    static Actions& instance();

    void setToolbarShortcutsEnabled(bool enabled);
    void setAnimationControlsEnabled(bool enabled);
    void updateAnimationControlsState();
    void setRenderModeActionsEnabled(bool enabled);
    void updateAnimationPopupState();
    void updateAudioMuteState();

    QAction* quit;
    QAction* undo;
    QAction* redo;
    QAction* cleanup;
    QAction* exportUSDZ;
    QAction* open;
    QAction* openRecent;
    QAction* resetLayout;
    QAction* save;
    QAction* saveAs;
    QAction* saveAsMinified;
    QAction* reloadStyleSheet;
    QAction* muteAudio;
    QAction* selectAnimation;
    QAction* stopAnimation;
    QAction* playingAnimation;
    QAction* rewindAnimation;
    QAction* forwardAnimation;
    QAction* about;
    QAction* newFile;
    QAction* showInspector;
    QAction* showHierarchy;
    QAction* showViewport;
    QAction* preferences;
    QAction* showViewportSettings;
    QAction* showStatistics;
    QAction* showResources;
    QAction* showLogger;
    QAction* reloadStyleSheetFromDisk;
    QAction* showThirdPartySoftware;
    QAction* showMITLicense;
    QAction* showPrivacyPolicy;
    QAction* newSampleFile;
    QAction* openDocumentation;
    QAction* showGrid;
    QAction* showBoundingBoxes;
    QAction* showSkinsAndBones;
    QActionGroup* viewportMode;
    QAction* renderMode;
    QAction* editMode;
    QActionGroup* tool;
    QAction* toolSelect;
    QAction* toolMove;
    QAction* toolRotate;
    QAction* toolScale;
    QAction* viewportResolution;
    QActionGroup* projection;
    QAction* projectionPerspective;
    QAction* projectionOrthographic;
    QActionGroup* editSpace;
    QAction* editSpaceWorld;
    QAction* editSpaceLocal;
    QAction* editSpaceToggle;
    QAction* cameraOrientationTop;
    QAction* cameraOrientationBottom;
    QAction* cameraOrientationRight;
    QAction* cameraOrientationLeft;
    QAction* cameraOrientationFront;
    QAction* cameraOrientationBack;
    QActionGroup* visualizationMode;
    QAction* visualizationModeDefaultShaded;
    QAction* visualizationModeWireframe;
    QList<QAction*> visualizationModeDebugShadings;
    QAction* createCameraFromView;
    QAction* focus;
    QAction* createScene;
    QAction* createNode;
    QAction* createCamera;
    QAction* createAudioEmitter;
    QAction* createLight;
    QAction* createMaterial;
    QAction* createTexture;
    QAction* createSampler;
    QAction* createMaterialVariant;
    QAction* importXmpPacket;
    QAction* createAnimation;
    QAction* createMesh;
    QAction* importImage;
    QAction* importTexture;
    QAction* importMaterial;
    QAction* import3DModel;
    QAction* importAudioSource;
    QAction* setDefaultScene;
    QAction* unsetDefaultScene;
    QAction* openScene;
    QAction* renderCapture;
    QAction* copy;
    QAction* cut;
    QAction* pasteLinked;
    QAction* pasteDeep;
    QAction* remove;
    QAction* rename;
    QAction* editImageExternally;

    // Automation
    QAction* removeSpecular;
    QAction* removeClearcoat;
    QAction* removeSheen;
    QAction* removeTransmission;
    QAction* removeVolume;
    QAction* removeIridescence;
    QAction* removeEmissiveStrength;
    QAction* removeIOR;
    QAction* removeSpecularGlossiness;
    QAction* removeUnlit;
    QAction* removeTextureTransform;
    QAction* removeLights;
    QAction* removeMaterialVariants;
    QAction* removeMeshQuantizationFromGlTF;
    QAction* removeDracoFromGlTF;
    QAction* applyDracoToGlTF;
    QAction* cleanExtensionsUsed;
    QAction* removeUnknownExtensions;
    QAction* removeExtras;
    QAction* removeNames;
    QAction* removeUnusedTexcoordAttributes;
    QAction* removeUnusedNodes;

private:
    Actions();

    void setupConnections();

    void createNodeFunction();
    void createSceneFunction();
    void createCameraFunction();
    void createLightFunction();
    void createMaterialFunction();
    void createAnimationFunction();
    void createTextureFunction();
    void createSamplerFunction();
    void createMeshFunction();
    void createMaterialVariantFunction();
    void showImportDialog(const QString& filter, const QString& caption, std::function<void(const QString&)> func);
    void importImageFunction();
    void importImageAsTexture();
    void import3DModelFunction();
    void importMaterialFromPbr();
    void importXmpPacketFunction();
    void removeExtension(const QString& extension);

    const std::vector<std::tuple<std::string, RenderMode>> m_debugOutputMapping = {
        { "Texture Coordinates 0", RenderMode::TexCoord_0 },
        { "Texture Coordinates 1", RenderMode::TexCoord_1 },
        { "Normal Texture", RenderMode::NormalTexture },
        { "Geometry Normal", RenderMode::NormalGeometry },
        { "Geometry Tangent ", RenderMode::Tangent },
        { "Geometry Bitangent ", RenderMode::Bitangent },
        { "Shading Normal", RenderMode::ShadingNormal },
        { "Alpha", RenderMode::Alpha },
        { "", RenderMode::None }, // Separator
        { "Occlusion", RenderMode::Occlusion },
        { "Emissive", RenderMode::Emissive },
        { "", RenderMode::None },
        { "Metallic Roughness", RenderMode::MetallicRoughness },
        { "    BaseColor", RenderMode::BaseColor },
        { "    Roughness", RenderMode::Roughness },
        { "    Metallic", RenderMode::Metallic },
        { "", RenderMode::None },
        { "Clearcoat", RenderMode::Clearcoat },
        { "    Clearcoat Factor", RenderMode::ClearcoatFactor },
        { "    Clearcoat Roughness", RenderMode::ClearcoatRoughness },
        { "    Clearcoat Normal", RenderMode::ClearcoatNormal },
        { "", RenderMode::None },
        { "Sheen", RenderMode::Sheen },
        { "    Sheen Color", RenderMode::SheenColor },
        { "    Sheen Roughness", RenderMode::SheenRoughness },
        { "", RenderMode::None },
        { "Specular", RenderMode::Specular },
        { "    Specular Factor", RenderMode::SpecularFactor },
        { "    Specular Color", RenderMode::SpecularColor },
        { "", RenderMode::None },
        { "Transmission/Volume", RenderMode::TransmissionVolume },
        { "    Transmission Factor", RenderMode::TransmissionFactor },
        { "    Volume Thickness", RenderMode::VolumeThickness },
        { "", RenderMode::None },
        { "Iridescence", RenderMode::Iridescence },
        { "    Iridescence Factor", RenderMode::IridescenceFactor },
        { "    Iridescence Thickness", RenderMode::IridescenceThickness },
    };
};
}
