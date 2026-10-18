#pragma once

#include "core/Vector.h"
#include "enums/GltfObjectSelectionScheme.h"
#include "enums/InteractorResult.h"
#include "helpers/FileFilters.h"

#include <string>
#include <utility>

namespace ux3d::gltf {
class Image;
}

namespace ux3d::filesystem {
class File;
class MemoryStream;
}

namespace ux3d::editor {
enum class AskForLocation : uint32_t {
    Always,
    IfNoPathSet
};

class SaveInteractor {
public:
    static SaveInteractor* getInstance();
    InteractorResult exportToUSD();
    InteractorResult saveProject(AskForLocation _behavior, bool _saveMinified);
    InteractorResult saveToMemoryStream(core::Vector<uint8_t>& _output, SaveFileFilter _type, GltfObjectSelectionScheme _selectionScheme);
    std::string saveImageToTemp(gltf::Image* image);

private:
    InteractorResult saveToFile(const filesystem::File& _file, SaveFileFilter _type,
        GltfObjectSelectionScheme _selectionScheme, bool _allowResourceOverwriting, bool _saveMinified);
    bool askForOverwritingPermissions();
    std::pair<std::string, std::string> askUserForPathAndType(const std::string& _initialPath, const QString& _filter);
    bool checkFileExtension(const std::string& _path, const std::string& _type);
    const SaveFileFilter getFileTypeFromPath(const std::string& _path);

    // default/deleted constructors etc.
public:
    ~SaveInteractor() = default;
    SaveInteractor(const SaveInteractor&) = delete;
    SaveInteractor(SaveInteractor&&) = delete;
    void operator=(const SaveInteractor&) = delete;
    void operator=(SaveInteractor&&) = delete;

private:
    SaveInteractor() = default;
};
}
