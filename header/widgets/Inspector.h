#pragma once

#include "widgets/CustomDockWidget.h"

class QQmlEngine;

namespace ux3d::editor {
class HotReloadQQuickWidget;

class Inspector : public CustomDockWidget {
    Q_OBJECT

public:
    explicit Inspector(QWidget* _Parent = nullptr, QQmlEngine* engine = nullptr);

private:
    HotReloadQQuickWidget* m_qmlWidget;
};
}
