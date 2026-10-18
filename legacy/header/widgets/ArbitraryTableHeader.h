#pragma once

#include <QHeaderView>
#include <QList>

namespace ux3d::editor {
class ArbitraryTableHeader : public QHeaderView {
public:
    using QHeaderView::QHeaderView;
    void setSectionWidget(int _sectionIndex, QWidget* _widget);

protected:
    void showEvent(QShowEvent* _event) final;
    void setModel(QAbstractItemModel* _model) final;

private:
    QList<QWidget*> m_sectionWidgets;
};
}
