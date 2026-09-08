#pragma once

#include <QDockWidget>

namespace ux3d::editor {
class CustomTitleWidget;

class CustomDockWidget : public QDockWidget {
    Q_OBJECT

private:
    CustomTitleWidget* m_pTitleBar = nullptr;
    QString m_Title;

public:
    CustomDockWidget(QWidget* _pParent = nullptr);

    const QString& getTitle() const;
    void setTitle(const QString& _title);
    CustomTitleWidget* getTitleBar() const;
    void setTitleBar(CustomTitleWidget* _pTitleBar);

private slots:
    void toggleMaximize();
    void setTitleButtonVisibility(bool _visible);
};
}
