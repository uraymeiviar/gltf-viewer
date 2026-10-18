#include "ValidationModel.h"
#include "model/ProjectManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

using namespace ux3d;
using namespace ux3d::editor;

ValidationModel::ValidationModel(QObject* parent)
    : TreeModel(parent)
{
    validationSubscription = model::ProjectManager::getInstance()->model.validation.distinct_until_changed().subscribe([this](const nlohmann::json& report) {
        nlohmann::json transformedReport;
        if (!report.contains("/issues/messages"_json_pointer)) {
            return;
        }
        nlohmann::json tree = { { "children", report.at("/issues/messages"_json_pointer) } };
        std::stable_sort(tree.at("children").begin(), tree.at("children").end(), [](const nlohmann::json& first, const nlohmann::json& second) {
            return first.at("severity").get<int>() < second.at("severity").get<int>();
        });
        updateTree(tree); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });

    connect(this, &ValidationModel::itemPushed, this, [](const Node& node) {

    });
}

ValidationModel ::~ValidationModel()
{
    validationSubscription.unsubscribe();
}

int ValidationModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant ValidationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }
    auto node = reinterpret_cast<Node*>(index.internalPointer());
    auto document = QJsonDocument::fromJson(QString(node->data.json.dump().c_str()).toUtf8());
    return document.object();
}

/*QVariant ValidationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QString data;
    switch (section) {
        case 0:
            data = "";
            break;
        case 1:
            data = "Code";
            break;
        case 2:
            data = "Description";
            break;
        case 3:
            data = "Location";
            break;
    }
    return data;
}
*/
