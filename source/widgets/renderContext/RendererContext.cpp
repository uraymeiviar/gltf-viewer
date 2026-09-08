#include "widgets/renderContext/RendererContext.h"
#include "Preferences.h"
#include "core/Logger.h"
#include "helpers/QtFileSystem.h"
#include "helpers/Utils.h"
#include "model/ProjectManager.h"
#include "renderer/CommandGenerator.h"
#include "renderer/PickingRenderLayer.h"
#include "renderer/editor/AudioRenderer.h"
#include "renderer/gl/ShaderCache.h"
#include "renderer/passes/TransparentPass.h"

#include <QOpenGLFramebufferObject>

using namespace ux3d;
using namespace ux3d::editor;

RenderParameters::RenderParameters()
{
    multiSampleCount = Preferences::getInstance()->getMSAAExp() <= 0 ? 0 : 1 << Preferences::getInstance()->getMSAAExp();
}

RendererContext::RendererContext()
    : m_renderState(&m_gl)
{
    auto* pm = model::ProjectManager::getInstance();
    pm->model.head.observe_on(pm->uiWorker).map([](const nlohmann::json&) { return std::monostate(); }).merge(pm->dirtyStateChanged.get_observable()).subscribe([this](std::monostate) { dirtyStateChanged(); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
}

RendererContext::~RendererContext()
{
    m_renderState.renderer.uninit();

    if (m_renderState.framebuffer != nullptr) {
        delete m_renderState.framebuffer;
        m_renderState.framebuffer = nullptr;
    }
    if (m_renderState.pickingFramebuffer != nullptr) {
        delete m_renderState.pickingFramebuffer;
        m_renderState.pickingFramebuffer = nullptr;
    }
    if (m_renderState.transmissionFramebuffer != nullptr) {
        delete m_renderState.transmissionFramebuffer;
        m_renderState.transmissionFramebuffer = nullptr;
    }
    if (m_renderState.selectionFramebuffer != nullptr) {
        delete m_renderState.selectionFramebuffer;
        m_renderState.selectionFramebuffer = nullptr;
    }

    cleanup();
}

void RendererContext::cleanup()
{
    if (m_renderState.commandGenerator != nullptr) {
        delete m_renderState.commandGenerator;
        m_renderState.commandGenerator = nullptr;
    }

    if (m_renderState.audioRenderer != nullptr) {
        delete m_renderState.audioRenderer;
        m_renderState.audioRenderer = nullptr;
    }

    if (m_renderState.pickingLayer != nullptr) {
        delete m_renderState.pickingLayer;
        m_renderState.pickingLayer = nullptr;
    }

    if (m_renderState.editorLayer != nullptr) {
        delete m_renderState.editorLayer;
        m_renderState.editorLayer = nullptr;
    }

    if (m_renderState.gltfLayer != nullptr) {
        delete m_renderState.gltfLayer;
        m_renderState.gltfLayer = nullptr;
    }
}

void RendererContext::setGltfRenderLayerParamsAndComGenFromParameters()
{
    if (m_renderState.gltfLayer != nullptr) {
        m_renderState.commandGenerator->setIBLEnabled(renderParameters.iblEnabled);
        m_renderState.gltfLayer->enableCubeMap(renderParameters.cubeMapEnabled);
        m_renderState.commandGenerator->setPunctualLightsEnabled(renderParameters.punctualLightsEnabled);
        m_renderState.commandGenerator->setToneMapping(renderParameters.toneMapping);
        m_renderState.commandGenerator->setExposure(renderParameters.exposure);
        m_renderState.commandGenerator->setEnvironmentMapRotation(renderParameters.envMapRotation);
        m_renderState.commandGenerator->setEnvironmentMapBlurNormalized(renderParameters.envMapBlur);
        m_renderState.commandGenerator->setEnvironmentMapIntensity(renderParameters.envMapIntensity);
        m_renderState.gltfLayer->renderSettings.enableSkinning = renderParameters.isSkinningEnabled;
        m_renderState.gltfLayer->renderSettings.enableMorphing = renderParameters.isMorphingEnabled;
        m_renderState.gltfLayer->renderSettings.enableSpecularGlossiness = renderParameters.isPbrSpecularGlossinessEnabled;
        m_renderState.gltfLayer->renderSettings.enableUnlit = renderParameters.isUnlitEnabled;
        m_renderState.gltfLayer->renderSettings.enableClearCoat = renderParameters.isClearCoatEnabled;
        m_renderState.gltfLayer->renderSettings.enableSheen = renderParameters.isSheenEnabled;
        m_renderState.gltfLayer->renderSettings.enableTransmission = renderParameters.isTransmissionEnabled;
        m_renderState.gltfLayer->renderSettings.enableVolume = renderParameters.isVolumeEnabled;
        m_renderState.gltfLayer->renderSettings.enableIOR = renderParameters.isIOREnabled;
        m_renderState.gltfLayer->renderSettings.enableSpecular = renderParameters.isSpecularEnabled;

        m_renderState.gltfLayer->setEnvironmentMap(renderParameters.envMapPath);
    }
}

void RendererContext::initGL(bool createAudio)
{
    core::logInfo("OpenGL Renderer: %s", m_gl.getString(GL_RENDERER));

    cleanup();

    filesystem::Path viewportHelperPath(QtFileSystem::instance(), ":/resources/png/Viewport_Helper");
    filesystem::Path transformationGizmosPath(QtFileSystem::instance(), ":/resources/models/");
    auto shaderSource = filesystem::Path(QtFileSystem::instance(), UX3D_RENDERER_RESOURCES_DIR) / "shaders";

    m_renderState.gltfLayer = new renderer::GltfRenderLayer(&m_gl, shaderSource / "ibl_sampler");
    m_renderState.editorLayer = new renderer::EditorRenderLayer(&m_gl, *m_renderState.gltfLayer, viewportHelperPath, transformationGizmosPath);
    m_renderState.pickingLayer = new renderer::PickingRenderLayer(&m_gl);
    if (createAudio) {
        // audio is not needed when e.g. capturing screenshots and only one openAL context at a time is supported
        m_renderState.audioRenderer = new renderer::AudioRenderer(*m_renderState.gltfLayer);
    }

    m_renderState.commandGenerator = new renderer::CommandGenerator(&m_gl, shaderSource);
}

void RendererContext::setDimensions(uint32_t _width, uint32_t _height)
{
    renderParameters.width = _width;
    renderParameters.height = _height;
    // this fixes black bars when a camera is selected in render mode (and select a camera without aspect ratio)
    dirtyStateChanged();
}

void RendererContext::paintGL(float deltaTime)
{
    if (m_renderState.editorLayer != nullptr) {
        m_renderState.editorLayer->m_wireframeColor = utils::getVec4FromColor(Preferences::getInstance()->getWireframeColor(), 1.0f);
        m_renderState.editorLayer->m_outlineColor = utils::getVec4FromColor(Preferences::getInstance()->getOutlineColor());
        m_renderState.editorLayer->m_boundingBoxColor = utils::getVec3FromColor(Preferences::getInstance()->getBoundingBoxColor());
        m_renderState.editorLayer->m_skeletonColor = utils::getVec3FromColor(Preferences::getInstance()->getSkeletonColor());
        m_renderState.editorLayer->m_outlineThickness = Preferences::getInstance()->getOutlineThickness();
        m_renderState.editorLayer->m_gridColor = utils::getVec4FromColor(Preferences::getInstance()->getGridColor(), 1.0f);
    }

    glm::dvec4 clearColor;
    if (renderParameters.clearColor.isValid()) {
        clearColor = utils::getVec4FromColor(renderParameters.clearColor);
    } else {
        clearColor = utils::getVec4FromColor(Preferences::getInstance()->getClearColor());
    }

    // Initialize framebuffers on first draw-call or if the framebuffer-related configuration changed.

    if (m_renderState.framebuffer == nullptr || m_renderState.framebuffer->width() != renderParameters.width || m_renderState.framebuffer->height() != renderParameters.height || m_renderState.framebuffer->format().samples() != renderParameters.multiSampleCount) {
        // Ensure all references to the framebuffer texture are updated in the render layer
        dirtyStateChanged();

        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(renderParameters.multiSampleCount);

        delete m_renderState.framebuffer;
        m_renderState.framebuffer = new QOpenGLFramebufferObject(renderParameters.width, renderParameters.height, format);
    }

    if (m_renderState.pickingFramebuffer == nullptr || m_renderState.pickingFramebuffer->width() != renderParameters.width || m_renderState.pickingFramebuffer->height() != renderParameters.height) {
        // Ensure all references to the pickingFramebuffer texture are updated in the render layer
        dirtyStateChanged();

        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

        delete m_renderState.pickingFramebuffer;
        m_renderState.pickingFramebuffer = new QOpenGLFramebufferObject(renderParameters.width, renderParameters.height, format);
    }

    if (m_renderState.transmissionFramebuffer == nullptr || m_renderState.transmissionFramebuffer->width() != renderParameters.width || m_renderState.transmissionFramebuffer->height() != renderParameters.height) {
        // Ensure all references to the transmissionFramebuffer texture are updated in the render layer
        dirtyStateChanged();

        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        // Restriction for mipmaps: no multisampling
        format.setMipmap(true);

        delete m_renderState.transmissionFramebuffer;
        m_renderState.transmissionFramebuffer = new QOpenGLFramebufferObject(renderParameters.width, renderParameters.height, format);
    }

    if (m_renderState.selectionFramebuffer == nullptr || m_renderState.selectionFramebuffer->width() != renderParameters.width || m_renderState.selectionFramebuffer->height() != renderParameters.height) {
        // Ensure all references to the selectionFramebuffer texture are updated in the render layer
        dirtyStateChanged();

        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

        delete m_renderState.selectionFramebuffer;
        m_renderState.selectionFramebuffer = new QOpenGLFramebufferObject(renderParameters.width, renderParameters.height, format);
    }

    m_renderState.framebuffer->bind();

    if (m_renderState.editorLayer != nullptr) {
        GLuint texID = m_renderState.selectionFramebuffer->texture();
        m_renderState.editorLayer->getEditorResourceManager().createFramebufferTexture(texID);

        std::function<void()> blitFramebuffer([this] {
            QOpenGLFramebufferObject::blitFramebuffer(m_renderState.selectionFramebuffer, m_renderState.framebuffer);
        });
        m_renderState.editorLayer->getEditorResourceManager().setBlitFramebufferFunction(blitFramebuffer, clearColor);
    }

    if (m_renderState.gltfLayer != nullptr) {
        GLuint texID = m_renderState.transmissionFramebuffer->texture();
        m_renderState.gltfLayer->getResourceManager().createFramebufferTexture(texID);

        m_renderState.gltfLayer->getResourceManager().setBlitFramebufferFunction([this] {
            QOpenGLFramebufferObject::blitFramebuffer(m_renderState.transmissionFramebuffer, m_renderState.framebuffer);
            m_gl.bindTexture(GL_TEXTURE_2D, m_renderState.transmissionFramebuffer->texture());
            m_gl.generateMipmap(GL_TEXTURE_2D);
        },
            clearColor);

        m_renderState.gltfLayer->getResourceManager().setTransmissiveTextureFunction([this] {
            m_gl.bindTexture(GL_TEXTURE_2D, m_renderState.transmissionFramebuffer->texture());
            m_gl.generateMipmap(GL_TEXTURE_2D);
        });

        m_renderState.framebuffer->bind();

        if (!m_renderState.gltfLayer->filterEnvironmentMap()) {
            core::logWarning("Environment map not ready");
        }
    }

    if (m_renderState.pickingLayer != nullptr) {
        m_renderState.pickingLayer->prePass = [this] {
            m_renderState.pickingFramebuffer->bind();
        };
        m_renderState.pickingLayer->postPass = [this] {
            m_renderState.framebuffer->bind();
        };
    }

    // reset state
    m_renderState.renderer.init(clearColor);

    // clear picking framebuffer
    m_renderState.pickingFramebuffer->bind();
    m_renderState.renderer.setViewportAndClear(renderParameters.width, renderParameters.height);

    // clear color framebuffer
    m_renderState.framebuffer->bind();
    m_renderState.renderer.setViewportAndClear(renderParameters.width, renderParameters.height);

    if (m_renderState.gltfLayer != nullptr) {
        // Update the mappings
        if (m_gltfRenderLayerNeedsUpdate) {
            if (renderParameters.pActiveGltf != nullptr) {
                // update the resources as present in the gltf
                m_renderState.gltfLayer->updateGltf(*renderParameters.pActiveGltf);

                if (renderParameters.pActiveGltfScene != nullptr) {
                    // configure the scene with the resources
                    m_renderState.gltfLayer->updateScene(*renderParameters.pActiveGltf, *renderParameters.pActiveGltfScene);
                }
            }

            if (renderParameters.pActiveGltf == nullptr || renderParameters.pActiveGltfScene == nullptr) {
                // if there is no scene or even no gltf opened, ensure the gltf
                // render layer doesn't contain any resources from the previous
                // scene
                m_renderState.gltfLayer->updateGltf(gltf::Gltf());
                m_renderState.gltfLayer->updateScene(gltf::Gltf(), gltf::Scene(0));
            }
        }

        float userGizmoScaling = Preferences::getInstance()->getGizmoScaling();
        m_renderState.editorLayer->setGizmoScaling(userGizmoScaling);
        m_renderState.editorLayer->update();

        // this must happen late, as the transform hierarchy is applied in this step
        m_renderState.gltfLayer->applyAnimationTime(renderParameters.animationTime); // animation & transform hierarchy & lights

        // update MaterialVariant
        m_renderState.gltfLayer->setMaterialVariant(renderParameters.materialVariantID);

        // Populate the scene
        auto [scene, reverseMapping] = m_renderState.gltfLayer->createScene(renderParameters.pActiveGltfScene);
        m_reverseMapping = std::move(reverseMapping);

        if (renderParameters.drawHelpers) {
            scene = m_renderState.editorLayer->applyLayer(std::move(scene));
            m_reverseMapping = m_renderState.editorLayer->applyLayer(std::move(m_reverseMapping));

            // picking layer
            scene = m_renderState.pickingLayer->applyLayer(std::move(scene));
        }

        // Generate commands
        m_renderState.commandGenerator->update(scene);
    }

    // audio should be updated after all other positional changes
    if (m_renderState.audioRenderer != nullptr) {
        if (renderParameters.pActiveGltfScene != nullptr) {
            if (m_gltfRenderLayerNeedsUpdate) {
                // configure the scene with the resources
                m_renderState.audioRenderer->updateScene(*renderParameters.pActiveGltf, *renderParameters.pActiveGltfScene);
            }
            m_renderState.audioRenderer->update(deltaTime);
        }
    }

    if (!m_renderState.renderer.draw({ m_renderState.commandGenerator })) {
        model::ProjectManager::getInstance()->setRendererEnabled(false);
        core::logError("Renderer: draw call was not successful");
    }

    m_gltfRenderLayerNeedsUpdate = false;
}

void RendererContext::dirtyStateChanged()
{
    m_gltfRenderLayerNeedsUpdate = true;
}
