// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "helpers/QtFileSystem.h"
#include "core/Defer.h"
#include "core/Platform.h"
#include "core/Slice.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

using namespace ux3d;
using namespace ux3d::filesystem;
using namespace ux3d::editor;

QtFileSystem& QtFileSystem::instance()
{
    static QtFileSystem m_filesystem;
    return m_filesystem;
}

filesystem::Path QtFileSystem::fromURI(const core::string& uri) const
{
    QUrl url = QUrl::fromLocalFile(uri.c_str());
    return Path(*this, url.toLocalFile().toStdString());
}

filesystem::Path QtFileSystem::getTempPath() const
{
    return this->toPath(QStandardPaths::writableLocation(QStandardPaths::TempLocation).toStdString());
}

filesystem::Path QtFileSystem::getAppDataPath() const
{
    return this->toPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString());
}

filesystem::Path QtFileSystem::getExecutableDirPath() const
{
    return this->toPath(QCoreApplication::applicationDirPath().toStdString());
}

bool QtFileSystem::isFile(const Path& path) const
{
    return QFileInfo(QString::fromStdString(path.toString())).isFile();
}

bool QtFileSystem::isDirectory(const Path& path) const
{
    return QFileInfo(QString::fromStdString(path.toString())).isDir();
}

bool QtFileSystem::isAbsolute(const Path& path) const
{
    return QFileInfo(QString::fromStdString(path.toString())).isAbsolute();
}

bool QtFileSystem::isRelative(const Path& path) const
{
    return QFileInfo(QString::fromStdString(path.toString())).isRelative();
}

bool QtFileSystem::isEmpty(const Path& path) const
{
    return path.toString().empty();
}

bool QtFileSystem::exists(const Path& path) const
{
    return QFileInfo(QString::fromStdString(path.toString())).exists();
}

core::Vector<Path> QtFileSystem::listContents(const Dir& _dir) const
{
    core::Vector<Path> contents;

    if (_dir.isDirectory() == false) {
        return contents;
    }

    QDir qtDir(QString::fromStdString(_dir.toString()));
    for (auto& entry : qtDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        (void)contents.emplace_back(*this, entry.filePath().toStdString());
    }

    return contents;
}

Path QtFileSystem::getParent(const Path& _path) const
{
    core::string pathStr = _path.toString();
    const unsigned long long lastSlashPos = pathStr.find_last_of("\\/");
    // return nothing if no folder present
    if (lastSlashPos == std::string::npos) {
        return toPath(".");
    }
    return toPath(pathStr.substr(0, lastSlashPos));
}

core::Vector<File> QtFileSystem::listFiles(const Dir& dir) const
{
    core::Vector<File> files;
    for (auto& entry : dir.listContents()) {
        if (entry.isFile()) {
            (void)files.push_back(entry);
        }
    }
    return files;
}

filesystem::PathResult QtFileSystem::create(const filesystem::Dir& directory) const
{
    QDir qDir(QString::fromStdString(directory.toString()));
    if (qDir.mkpath(".") == false) {
        return PathErrorE::SystemError;
    }

    return PathResult::Success;
}

filesystem::PathResult ux3d::editor::QtFileSystem::remove(const filesystem::Path& _path) const
{
    if (_path.isFile()) {
        QFile file(QString::fromStdString(_path.toString()));
        if (file.exists() == false)
            return filesystem::PathErrorE::NotFound;
        return file.remove() ? filesystem::PathResult::Success : filesystem::PathErrorE::SystemError;
    } else if (_path.isDirectory()) {
        QDir dir(QString::fromStdString(_path.toString()));
        if (dir.exists() == false)
            return filesystem::PathErrorE::NotFound;
        return dir.removeRecursively() ? filesystem::PathResult::Success : filesystem::PathErrorE::SystemError;
    }
    return filesystem::PathErrorE::PathIsNotADirectory;
}

Path QtFileSystem::getChild(const Path& path, const core::string& _name) const
{
    QDir dir(QString::fromStdString(path.toString() + "/" + _name));
    return Path(*this, dir.path().toStdString());
}

core::string QtFileSystem::getName(const Path& _path) const
{
    return QFileInfo(QString::fromStdString(_path.toString())).fileName().toStdString();
}

core::string QtFileSystem::getStem(const filesystem::File& _file) const
{
    return QFileInfo(QString::fromStdString(_file.toString())).baseName().toStdString();
}

core::string QtFileSystem::getExtension(const filesystem::File& _file) const
{
    auto suffix = QFileInfo(QString::fromStdString(_file.toString())).suffix().toStdString();
    if (!suffix.empty()) {
        return "." + suffix;
    }
    return suffix;
}

FileLoadResult QtFileSystem::load(const File& _file, core::Vector<uint8_t>& _data) const
{
    if (!sandboxGetAccessWithBookmark(_file)) {
        return FileLoadResultE::SandboxError;
    }
    const core::ScopeGuard deferSandboxRelease = [&] { sandboxReleaseAccessWithBookmark(_file); };

    QFile file(QString::fromStdString(_file.toString()));
    if (!file.open(QIODevice::ReadOnly)) {
        return FileLoadResultE::CannotOpenFile;
    }
    // The actual size of the file can change under Qt and Windows between resizing and actually reading the file.
    // Therefore, we need to check the the vector and file size separately.
    const qint64 fileSize = file.size();
    if (!_data.resize(fileSize)) {
        return FileLoadResultE::AllocationFailed;
    }
    const qint64 readSize = file.read(reinterpret_cast<char*>(_data.data()), _data.size());
    if (readSize == -1 || fileSize != readSize) {
        return FileLoadResultE::AllocationFailed;
    }
    file.close();

    return FileLoadResult::Success;
}

FileLoadResult QtFileSystem::load(const File& _file, core::Slice<uint8_t>& _data) const
{
    if (!sandboxGetAccessWithBookmark(_file)) {
        return FileLoadResultE::SandboxError;
    }
    const core::ScopeGuard deferSandboxRelease = [&] { sandboxReleaseAccessWithBookmark(_file); };

    QFile file(QString::fromStdString(_file.toString()));
    if (!file.open(QIODevice::ReadOnly)) {
        return FileLoadResultE::CannotOpenFile;
    }

    (void)_data.resize(file.size(), _data.getAllocator());
    file.read(reinterpret_cast<char*>(_data.data()), file.size());
    file.close();
    return FileLoadResult::Success;
}

FileLoadResult QtFileSystem::load(const File& _file, core::string& _text) const
{
    if (!sandboxGetAccessWithBookmark(_file)) {
        return FileLoadResultE::SandboxError;
    }
    const core::ScopeGuard deferSandboxRelease = [&] { sandboxReleaseAccessWithBookmark(_file); };

    QFile file(QString::fromStdString(_file.toString()));
    if (!file.open(QIODevice::ReadOnly)) {
        return FileLoadResultE::CannotOpenFile;
    }
    _text.resize(file.size());
    file.read(reinterpret_cast<char*>(_text.data()), file.size());
    file.close();
    return FileLoadResult::Success;
}

FileSaveResult QtFileSystem::save(const File& _file, const uint8_t* _data, const size_t _byteLength) const
{
    if (!sandboxGetAccessWithBookmark(_file)) {
        return FileSaveResultE::SandboxError;
    }
    const core::ScopeGuard deferSandboxRelease = [&] { sandboxReleaseAccessWithBookmark(_file); };

    QFile file(QString::fromStdString(_file.toString()));
    if (!file.open(QIODevice::WriteOnly)) {
        return FileSaveResultE::CannotOpenFile;
    }

    file.write(reinterpret_cast<const char*>(_data), _byteLength);
    file.close();
    return FileSaveResult::Success;
}

FileSaveResult QtFileSystem::save(const File& _file, const core::Vector<uint8_t>& _data) const
{
    return this->save(_file, _data.data(), _data.size());
}

FileSaveResult QtFileSystem::save(const File& _file, const core::Slice<uint8_t>& _data) const
{
    return this->save(_file, _data.data(), _data.size());
}

FileSaveResult QtFileSystem::save(const File& _file, const core::string& _text) const
{
    return this->save(_file, reinterpret_cast<const uint8_t*>(_text.data()), _text.size());
}

filesystem::FileSaveResult QtFileSystem::append(const File& _file, const core::string& _text) const
{
    QFile file(QString::fromStdString(_file.toString()));
    if (!file.open(QIODevice::Append))
        return FileSaveResultE::CannotOpenFile;
    file.write(_text.data(), _text.size());
    file.close();
    return FileSaveResult::Success;
}

bool QtFileSystem::isQrc(const filesystem::File& file)
{
    return QString::fromStdString(file.toString()).startsWith(":");
}

QUrl QtFileSystem::getQUrl(const filesystem::Path& path)
{
    return QUrl::fromLocalFile(QString::fromStdString(path.toString()));
}
