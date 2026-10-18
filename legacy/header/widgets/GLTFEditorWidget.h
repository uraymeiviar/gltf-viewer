#pragma once

#include "GLTFRendererWidget.h"
#include "gltf/Utils.h"

class QAction;

namespace ux3d::editor {
struct SelectionInfo;
}

namespace ux3d::editor {
class GLTFEditorWidget : public GLTFRendererWidget {
    Q_OBJECT

public:
    explicit GLTFEditorWidget(QWidget* _Parent = nullptr);
    ~GLTFEditorWidget() override;

    bool isEditMode() const { return m_EditMode; };
    void setEditMode(bool editMode);
    void performPickingInNextFrame() override;
    void setGizmoVisibility(bool _isVisible) override;
    void showGrid(bool _showGrid);
    void setGizmoSpace(GizmoSpace gizmoSpace) override;
    GizmoSpace getGizmoSpace() const override;

protected:
    void paintGL() override;

private:
    // Highlight
    gltf::Node* m_pSelectedNode = nullptr;

    bool m_EditMode = true;

    void setupActions();
    void focusSelectedNode();

private slots:
    void receiveSelection(const std::vector<SelectionInfo>& _selection);
};
}
