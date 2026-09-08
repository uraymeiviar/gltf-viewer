#pragma once

#include "QPushButton"

namespace ux3d::editor {
struct BackPathElement;

class PushButton_BackPath : public QPushButton {
    Q_OBJECT
private:
    const BackPathElement& m_backPathElement;

public:
    PushButton_BackPath(QWidget* _pParent, const BackPathElement& _backPathElement)
        : QPushButton(_pParent)
        , m_backPathElement(_backPathElement)
    {
        connect(this, &PushButton_BackPath::clicked, this, &PushButton_BackPath::onClicked);
    }

    const BackPathElement& getBackPathElement() { return m_backPathElement; };

private slots:
    void onClicked()
    {
        emit clickedButton(m_backPathElement);
    }

signals:
    void clickedButton(const BackPathElement& _backStackElement);
};
}
