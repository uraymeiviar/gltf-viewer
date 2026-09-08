#pragma once

#include "SelectionInfo.h"
#include <QObject>
#include <nlohmann/json.hpp>
#include <vector>

namespace ux3d::editor {
class SelectionManager : public QObject {
    Q_OBJECT

public:
    static SelectionManager* getInstance();
    SelectionManager(SelectionManager const&) = delete;
    void operator=(SelectionManager const&) = delete;
    const std::vector<SelectionInfo>& getCurrentSelection() const { return m_selectedObjects; }
    const nlohmann::json& getCurrentJsonSelection() const { return m_selection; }

signals:
    void selectObjects(const std::vector<SelectionInfo>& _selection);

private:
    SelectionManager();
    ~SelectionManager() = default;

    void editImageExternally();
    void openSelectedScene();
    void setDefaultScene();
    void unsetDefaultScene();
    std::vector<SelectionInfo> m_selectedObjects = {};

    nlohmann::json m_selection;
    nlohmann::json m_uuidPaths;
    nlohmann::json m_headRaw;
};
}
