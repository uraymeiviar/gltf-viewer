#pragma once

struct ClipboardContext {
    enum class Mode {
        Undefined,
        Copy,
        Cut
    };

    enum class Source {
        Undefined,
        Hierarchy,
        Content,
        Inspector
    };

    Mode mode = Mode::Undefined;
    Source source = Source::Undefined;
};
