#pragma once

#include "CustomDockWidget.h"
#include "interfaces/GltfInteractorOutput.h"
#include "interfaces/LoadInteractorOutput.h"
#include "interfaces/RenderSettingInteractorOutput.h"
#include "model/EnvironmentMapsBinding.h"
#include "model/IblSamplerTaskCollectionBinding.h"
#include "model/MaterialVariantsListBinding.h"
#include <QColor>
#include <QQmlEngine>
#include <QString>
#include <string>

namespace ux3d::editor {

class HotReloadQQuickWidget;

class ViewportSettings : public CustomDockWidget,
                         public LoadInteractorOutput,
                         public GltfInteractorOutput,
                         public RenderSettingInteractorOutput,
                         public model::IEnvironmentMapsPresentation,
                         public model::IIblSamplerTaskPresentation,
                         public model::IIblSamplerTaskCollectionPresentation,
                         public model::IMaterialVariantsListPresentation {

    Q_OBJECT

    // Environment
    Q_PROPERTY(bool showBackground READ getShowBackground WRITE setShowBackground NOTIFY showBackgroundChanged)
    Q_PROPERTY(QColor backgroundColor READ getBackgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(int blur READ getBlur WRITE setBlur NOTIFY blurChanged)
    Q_PROPERTY(QString frontFace READ getFrontFace WRITE setFrontFace NOTIFY frontFaceChanged)
    Q_PROPERTY(QString currentHDRFile READ getCurrentHDRFile WRITE setCurrentHDRFile NOTIFY currentHDRFileChanged)

    // Light
    Q_PROPERTY(bool environmentLighting READ getEnvironmentLighting WRITE setEnvironmentLighting NOTIFY environmentLightingChanged)
    Q_PROPERTY(float environmentLightingIntensity READ getEnvironmentLightingIntensity WRITE setEnvironmentLightingIntensity NOTIFY environmentLightingIntensityChanged)
    Q_PROPERTY(bool punctualLights READ getPunctualLights WRITE setPunctualLights NOTIFY punctualLightsChanged)

    // Post Processing
    Q_PROPERTY(QString toneMapping READ getToneMapping WRITE setToneMapping NOTIFY toneMappingChanged)
    Q_PROPERTY(float exposure READ getExposure WRITE setExposure NOTIFY exposureChanged)

    // Extensions
    Q_PROPERTY(bool pbrSpecularGlossiness READ getPbrSpecularGlossiness WRITE setPbrSpecularGlossiness NOTIFY pbrSpecularGlossinessChanged)
    Q_PROPERTY(bool unlit READ getUnlit WRITE setUnlit NOTIFY unlitChanged)
    Q_PROPERTY(bool clearCoat READ getClearCoat WRITE setClearCoat NOTIFY clearCoatChanged)
    Q_PROPERTY(bool sheen READ getSheen WRITE setSheen NOTIFY sheenChanged)
    Q_PROPERTY(bool transmission READ getTransmission WRITE setTransmission NOTIFY transmissionChanged)
    Q_PROPERTY(bool volume READ getVolume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool iridescence READ getIridescence WRITE setIridescence NOTIFY iridescenceChanged)
    Q_PROPERTY(bool ior READ getIor WRITE setIor NOTIFY iorChanged)
    Q_PROPERTY(bool specular READ getSpecular WRITE setSpecular NOTIFY specularChanged)
    Q_PROPERTY(bool emissiveStrength READ getEmissiveStrength WRITE setEmissiveStrength NOTIFY emissiveStrengthChanged)

public:
    explicit ViewportSettings(QWidget* parent, QQmlEngine* engine);
    ~ViewportSettings();

    void presentEnvironmentMaps(core::Vector<model::EnvironmentMapBinding>&& environmentMaps) final;
    void presentIblSamplerTaskIsDone(const bool isDone) final;
    void presentTaskCreated(model::IblSamplerTaskBinding&& taskBinding) final;
    void presentGltfOpen(gltf::Gltf*) final;
    void presentGltfImport(gltf::Gltf*) final;
    void presentGltfReloaded(gltf::Gltf* _pGltf) final { m_pMaterialVariantsList.refresh(); }
    void presentMaterialVariantsSelected(int id, core::Vector<const core::string> materialVariantNames) final;

    // Environment
    bool getShowBackground() const { return m_showBackground; };
    void setShowBackground(bool showBackground);

    const QColor getBackgroundColor() const { return m_backgroundColor; };
    void setBackgroundColor(QColor color);
    Q_INVOKABLE void openBackgroundColorDialog();

    int getBlur() { return m_blur; };
    void setBlur(int blur);

    float getEnvironmentLightingIntensity() const { return m_environmentLightingIntensity; };
    void setEnvironmentLightingIntensity(float environmentLighting);

    QString getCurrentHDRFile() { return m_currentHDRFile; }
    void setCurrentHDRFile(QString newHDRFile);
    void refreshCurrentEnvironmentMap(const std::string& newHDRFile);

    Q_INVOKABLE void addNewHDRFile();
    Q_INVOKABLE void deleteCurrentHDRFile();

    QString getFrontFace() { return m_frontFace; };
    void setFrontFace(QString frontFace);

    // Light
    bool getEnvironmentLighting() const { return m_environmentLighting; };
    void setEnvironmentLighting(bool environmentLighting);
    bool getPunctualLights() const { return m_punctualLights; };
    void setPunctualLights(bool state);

    // Post Processing
    QString getToneMapping() const;
    void setToneMapping(const QString toneMapping);
    float getExposure() const { return m_exposure; };
    void setExposure(float exposure);

    // Extensions
    void setPbrSpecularGlossiness(bool isEnabled);
    bool getPbrSpecularGlossiness() { return m_pbrSpecularGlossiness; }
    void setUnlit(bool isEnabled);
    bool getUnlit() { return m_unlit; }
    void setClearCoat(bool isEnabled);
    bool getClearCoat() { return m_clearCoat; }
    void setSheen(bool isEnabled);
    bool getSheen() { return m_sheen; }
    void setTransmission(bool isEnabled);
    bool getTransmission() { return m_transmission; }
    void setVolume(bool isEnabled);
    bool getVolume() { return m_volume; }
    void setIridescence(bool isEnabled);
    bool getIridescence() { return m_iridescence; }
    void setIor(bool isEnabled);
    bool getIor() { return m_ior; }
    void setSpecular(bool isEnabled);
    bool getSpecular() { return m_specular; }
    void setEmissiveStrength(bool isEnabled);
    bool getEmissiveStrength() { return m_emissiveStrength; }

    // Variants
    Q_INVOKABLE void setMaterialVariants(int index);

signals:
    void environmentMapSelected(model::EnvironmentMapBinding& environmentMapBinding);

    // Environments
    void showBackgroundChanged();
    void backgroundColorChanged();
    void blurChanged();
    void environmentLightingIntensityChanged();
    void frontFaceChanged();
    void setupHDRComboBox(QList<QString> hdrFileList);
    void currentHDRFileChanged();

    // Light
    void environmentLightingChanged();
    void punctualLightsChanged();

    // Post Processing
    void toneMappingChanged();
    void exposureChanged();

    // Extensions
    void pbrSpecularGlossinessChanged();
    void unlitChanged();
    void clearCoatChanged();
    void sheenChanged();
    void transmissionChanged();
    void volumeChanged();
    void iridescenceChanged();
    void iorChanged();
    void specularChanged();
    void emissiveStrengthChanged();

    // Variants
    void materialVariantsChanged(int selectedIndex, QList<QString> variantsList);

private:
    const QString convertAngleToFace(uint32_t _angle);
    uint32_t convertFaceToAngle(const QString& _face);

    HotReloadQQuickWidget* m_qmlWidget;

    // Environments
    bool m_showBackground = true;
    QColor m_backgroundColor = QColor(0, 0, 0);
    int m_blur = 0;
    float m_environmentLightingIntensity = 1.0;

    model::EnvironmentMapsBinding m_environmentMaps;
    model::EnvironmentMapBinding m_defaultEnvironmentMap;
    model::EnvironmentMapBinding* m_currentEnvironmentMap = nullptr;
    core::Vector<model::EnvironmentMapBinding> m_environmentMapBindings;
    QString m_currentHDRFile = "";
    model::IblSamplerTaskBinding m_iblSamplerTaskBinding;
    model::IblSamplerTaskCollectionBinding m_iblSamplerTaskCollectionBinding;
    QTimer* m_hdrLoadTimer = nullptr;

    QString m_frontFace = "";

    // Light
    bool m_environmentLighting = true;
    bool m_punctualLights = true;

    // Post Processing
    renderer::ToneMappingsEnum m_toneMapping = renderer::ToneMappingsEnum::None;
    float m_exposure = 1.0f;

    // Extensions
    bool m_pbrSpecularGlossiness = true;
    bool m_unlit = true;
    bool m_clearCoat = true;
    bool m_sheen = true;
    bool m_transmission = true;
    bool m_volume = true;
    bool m_volumePrevState = true; // state of volume entry before transmission was disabled
    bool m_iridescence = true;
    bool m_ior = true;
    bool m_specular = true;
    bool m_emissiveStrength = true;

    // Variants
    model::MaterialVariantsListBinding m_pMaterialVariantsList;

public slots:
    void topLevelChanged(bool topLevel);
};
}
