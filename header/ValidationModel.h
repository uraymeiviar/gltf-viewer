#include "TreeModel.h"
#include <rxcpp/rx.hpp>

namespace ux3d::editor {
class ValidationModel : public TreeModel {
    Q_OBJECT

public:
    ValidationModel(QObject* parent = nullptr);
    ~ValidationModel();
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    //    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    rxcpp::subscription validationSubscription;
};

}