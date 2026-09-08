#include "filesystem/FileSystem.h"
#include "core/Platform.h"

using namespace ux3d;
using namespace ux3d::filesystem;

#ifndef UX3D_IS_APPLE
// default implementations
bool IFileSystem::sandboxIsExempt(const filesystem::Path& file) { return true; }
bool IFileSystem::sandboxHasBookmark(const filesystem::File&) { return true; }
void IFileSystem::sandboxSaveBookmark(const filesystem::File&) { }
bool IFileSystem::sandboxRequestAccess(const filesystem::Path&) { return true; }
bool IFileSystem::sandboxGetAccessWithBookmark(const filesystem::File&) { return true; }
void IFileSystem::sandboxReleaseAccessWithBookmark(const filesystem::File&) { }
#endif
