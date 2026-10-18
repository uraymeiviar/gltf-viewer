#include "helpers/FileFilters.h"
#include "core/Algorithms.h"
#include "core/Vector.h"

namespace open {
constexpr auto gltfAndGlbAndGestalt = "glTF, glb, and Gestaltor project files (*.gltf *.glb *.gestalt)";
constexpr auto gestalt = "Gestaltor project files (*.gestalt)";
constexpr auto gltfAndGlb = "glTF & glb files (*.gltf *.glb)";
constexpr auto gltf = "glTF files (*.gltf)";
constexpr auto glb = "glb files (*.glb)";
constexpr auto all = "All files (*)";
constexpr auto allModel = "All model files (*.gltf *.glb *.blend *.3ds *.dxf *.fbx *.gltf *.obj *.ply *.stl *.x3d *.gestalt)";
}

namespace save {
constexpr auto gltf = "gltf (*.gltf)";
constexpr auto glb = "glb (*.glb)";
}

constexpr auto separator = ";;";

QString ux3d::editor::openFileFiltersToString(const OpenFileFilters& _filters)
{
    QString result = "";

    const std::vector<std::pair<OpenFileFilter, QString>> availableFilters = {
        { OpenFileFilter::GltfAndGlbAndGestaltFiles, open::gltfAndGlbAndGestalt },
        { OpenFileFilter::GltfAndGlbFiles, open::gltfAndGlb },
        { OpenFileFilter::GltfFiles, open::gltf },
        { OpenFileFilter::GlbFiles, open::glb },
        { OpenFileFilter::AllFiles, open::all },
        { OpenFileFilter::AllModelFiles, open::allModel }
    };

    for (auto& availableFilter : availableFilters) {
        if (core::hasFlag(_filters, availableFilter.first)) {
            if (result.isEmpty() == false) {
                result += separator;
            }
            result += availableFilter.second;
        }
    }
    return result;
}

QString ux3d::editor::openFileFiltersToString(OpenFileFilter _a, OpenFileFilter _b, OpenFileFilter _c)
{
    OpenFileFilters combined = _a;
    combined |= _b;
    combined |= _c;

    return openFileFiltersToString(combined);
}

ux3d::editor::SaveFileFilter ux3d::editor::saveFileFilterFromString(const QString& _filter)
{
    if (_filter == save::gltf) {
        return SaveFileFilter::Gltf;
    }
    if (_filter == save::glb) {
        return SaveFileFilter::Glb;
    }

    return SaveFileFilter::None;
}

QString ux3d::editor::saveFileFilterToString(const SaveFileFilter& _filter)
{
    if (_filter == SaveFileFilter::Gltf) {
        return save::gltf;
    }
    if (_filter == SaveFileFilter::Glb) {
        return save::glb;
    }

    return "";
}

QString ux3d::editor::saveFileFiltersToString(const std::vector<SaveFileFilter>& _filters)
{
    QString result = "";
    for (const auto filter : _filters) {
        QString filterString = saveFileFilterToString(filter);
        if (filterString.isEmpty() == false) {
            if (result.isEmpty() == false) {
                result += separator;
            }
            result += saveFileFilterToString(filter);
        }
    }
    return result;
}
