#include "widgets/GLTFEditorWidget.h"
#include "Actions.h"
#include "SelectionManager.h"
#include "widgets/renderContext/RendererContext.h"

#include <QAction>
#include <QOpenGLFramebufferObject>

using namespace ux3d;
using namespace ux3d::editor;

GLTFEditorWidget::GLTFEditorWidget(QWidget* _Parent)
    : GLTFRendererWidget(_Parent)
{
    setupActions();

    connect(SelectionManager::getInstance(), &SelectionManager::selectObjects, this, &GLTFEditorWidget::receiveSelection);
}

GLTFEditorWidget::~GLTFEditorWidget()
{
    makeCurrent();
}

void GLTFEditorWidget::performPickingInNextFrame()
{
    // when render/edit mode use the same type of camera, this function is called for both modes. However, currently we only want picking in edit mode.
    if (m_EditMode) {
        m_isPicking = true;
    }
}

void GLTFEditorWidget::setEditMode(bool editMode)
{
    m_EditMode = editMode;
    m_useViewportResolution = !editMode;
    if (!editMode) {
        m_pSelectedNode = nullptr;
        if (m_rendererContext->getRenderState().editorLayer != nullptr) {
            m_rendererContext->getRenderState().editorLayer->m_selectedItems.clear();
        }
    } else {
        receiveSelection(SelectionManager::getInstance()->getCurrentSelection());
    }
}

void GLTFEditorWidget::showGrid(bool _showGrid)
{
    if (m_rendererContext->getRenderState().editorLayer != nullptr) {
        m_rendererContext->getRenderState().editorLayer->showGrid = _showGrid;
    }
}

void GLTFEditorWidget::receiveSelection(const std::vector<SelectionInfo>& _selection)
{
    m_rendererContext->getRenderState().editorLayer->m_selectedItems.clear();
    m_rendererContext->getRenderState().editorLayer->m_selectedMaterials.clear();

    for (const auto& selection : _selection) {
        std::optional<std::variant<const gltf::Node*, const gltf::Mesh*, const gltf::Light*, const gltf::Camera*, const gltf::Material*, const gltf::Primitive*>> selectedItem;
        if (selection.type == GltfPropertyType::Nodes) {
            selectedItem.emplace(selection.pointer.unsafeGet<gltf::Node*>());
        } else if (selection.type == GltfPropertyType::Materials) {
            const auto* material = selection.pointer.unsafeGet<gltf::Material*>();
            selectedItem.emplace(material);
            if (m_rendererContext->getRenderState().editorLayer != nullptr && selectedItem.has_value()) {
                m_rendererContext->getRenderState().editorLayer->m_selectedMaterials.insert(
                    material);
            }

        } else if (selection.type == GltfPropertyType::Meshes) {
            selectedItem.emplace(selection.pointer.unsafeGet<gltf::Mesh*>());
        } else if (selection.type == GltfPropertyType::Primitives) {
            selectedItem.emplace(selection.pointer.unsafeGet<gltf::Primitive*>());
        } else if (selection.type == GltfPropertyType::Lights) {
            selectedItem.emplace(selection.pointer.unsafeGet<gltf::Light*>());
        } else if (selection.type == GltfPropertyType::Cameras) {
            selectedItem.emplace(selection.pointer.unsafeGet<gltf::Camera*>());
        }

        if (m_rendererContext->getRenderState().editorLayer != nullptr && selectedItem.has_value()) {
            m_rendererContext->getRenderState().editorLayer->m_selectedItems.insert(
                selectedItem.value());
        }
    }

    if (_selection.size() > 0 && _selection[0].type == GltfPropertyType::Nodes) {
        m_pSelectedNode = _selection[0].pointer.unsafeGet<gltf::Node*>();
    } else {
        m_pSelectedNode = nullptr;
    }

    setGizmoVisibility(m_input.getInputToolState() != InputToolState::Selection);
}

void GLTFEditorWidget::paintGL()
{
    m_input.getNodeActor()->setNode(m_EditMode ? m_pSelectedNode : nullptr);

    m_rendererContext->renderParameters.drawHelpers = m_EditMode;
    GLTFRendererWidget::paintGL();
    m_rendererContext->renderParameters.drawHelpers = false;
}

void GLTFEditorWidget::setupActions()
{
    auto* focusNodeAction = Actions::instance().focus;
    connect(focusNodeAction, &QAction::triggered, this, &GLTFEditorWidget::focusSelectedNode);
}

void GLTFEditorWidget::focusSelectedNode()
{
    if (m_pSelectedNode == nullptr) {
        return;
    }
    if (m_rendererContext->getRenderState().gltfLayer != nullptr) {
        m_cameraActor.presentFocusNode(m_rendererContext->getRenderState().gltfLayer, m_pSelectedNode);
    }
}

void GLTFEditorWidget::setGizmoVisibility(bool _isVisible)
{
    if (_isVisible) {
        if (m_rendererContext->getRenderState().editorLayer) {
            m_rendererContext->getRenderState().editorLayer->setGizmoedNode(m_pSelectedNode, &m_gizmoState);
        }
    } else {
        if (m_rendererContext->getRenderState().editorLayer) {
            m_rendererContext->getRenderState().editorLayer->setGizmoedNode(m_pSelectedNode, nullptr);
        }
    }
}

void GLTFEditorWidget::setGizmoSpace(GizmoSpace gizmoSpace)
{
    if (m_rendererContext->getRenderState().editorLayer) {
        m_rendererContext->getRenderState().editorLayer->m_gizmoSpace = gizmoSpace;
    }
}

GizmoSpace GLTFEditorWidget::getGizmoSpace() const
{
    if (m_rendererContext->getRenderState().editorLayer) {
        return m_rendererContext->getRenderState().editorLayer->m_gizmoSpace;
    }
    return GizmoSpace::Global;
}
