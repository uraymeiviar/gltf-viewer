// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include <stdint.h>

namespace ux3d::model {
// Binding between the presentation and the application layer
template <class IPresentation>
class TBinding {
public:
    // this id uniquely identifies a data element and is shared between view controllers that
    // operate on the same data object
    using ID = uint64_t;

    TBinding(IPresentation* presentation = nullptr)
        : m_presentation(presentation)
    {
    }

    // TBinding is a pure virtual class
    virtual ~TBinding() = 0;

    // getID returns a unique ID for the model's data
    virtual ID getID() const = 0;

    // isPresentable returns true if there is no presentation for this binding. It then is write only.
    virtual bool isPresentable() const { return m_presentation != nullptr; };
    // bind a new presentation to this binding. Can be helpful to late init the binding.
    virtual void bind(IPresentation* _presentation) { m_presentation = _presentation; }
    // getPresentation to access the bound presentation.
    virtual IPresentation* getPresentation() const { return m_presentation; }

protected:
    // A pointer to the presentation layer instance that this binding refers to.
    // It can be nullptr if the binding is one directional
    IPresentation* m_presentation = nullptr;
};

template <class IPresentation>
TBinding<IPresentation>::~TBinding<IPresentation>()
{
    // compulsory virtual destructor definition
}
}
