#include "reactor/Validation.h"

#include "subprocess.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef UX3D_IS_APPLE
#include <limits.h>
#include <mach-o/dyld.h>
#endif

#ifdef UX3D_IS_WINDOWS
#include "reactor/Utils.h"
#include <WinNT.h>
#include <libloaderapi.h>
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

using namespace ux3d;
using namespace ux3d::reactor;

nlohmann::json validation::validate(const std::string& path, bool checkResources)
{
    subprocess::CommandLine commandLine;
#ifdef UX3D_IS_WINDOWS
    char dllPath[MAX_PATH];
    if (GetModuleFileNameA((HINSTANCE)&__ImageBase, dllPath, MAX_PATH)) {
        std::string pathToDLL(dllPath);
        reactor::utils::searchAndReplace(pathToDLL, "Reactor.dll", "gltf_validator.exe");
        commandLine.push_back(pathToDLL);
    }
#endif
#ifdef UX3D_IS_APPLE
    char buf[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    if (_NSGetExecutablePath(buf, &bufsize) != 0) {
        throw new std::runtime_error("Could not get executable path on macOS");
    }
    auto executablePath = std::filesystem::path(buf);
    executablePath = executablePath.replace_filename("gltf_validator");
    commandLine.push_back(executablePath.generic_string());
#endif
#ifdef UX3D_IS_LINUX
    auto executablePath = std::filesystem::canonical("/proc/self/exe");
    executablePath = executablePath.replace_filename("gltf_validator");
    commandLine.push_back(executablePath.generic_string());
#endif
    commandLine.push_back("--stdout");
    if (!checkResources) {
        commandLine.push_back("--no-validate-resources");
    }
    commandLine.push_back(path);

    auto completion = subprocess::RunBuilder { std::move(commandLine) }
                          .cout(subprocess::PipeOption::pipe)
                          .cerr(subprocess::PipeOption::pipe)
                          .run();

    if (completion.cout.empty()) {
        return nlohmann::json::object();
    }
    auto result = nlohmann::json::parse(completion.cout);
    sanitize(result);
    return result;
}

nlohmann::json validation::validate(const nlohmann::json& gltf)
{
    // only validate the gltf --> proof of concept
    auto tmpGltfPath = std::filesystem::temp_directory_path() / ".log.gltf";

    std::ofstream gltfFile(tmpGltfPath);
    gltfFile << gltf.dump();
    gltfFile.close();

    auto result = validate(tmpGltfPath.string(), false);
    try {
        std::filesystem::remove(tmpGltfPath);
    } catch (...) {
    }
    return result;
}

void validation::sanitize(nlohmann::json& input)
{
    nlohmann::json::json_pointer messagesPointer("/issues/messages");
    if (input.contains(messagesPointer)) {
        int warningCounter = input.at("/issues/numWarnings"_json_pointer);
        nlohmann::json& messagesArray = input.at(messagesPointer);
        for (auto iter = messagesArray.begin(); iter != messagesArray.end();) {
            if (iter->value("message", "") == "Invalid value 'image/ktx2'. Valid values are ('image/jpeg', 'image/png').") {
                iter = messagesArray.erase(iter);
                --warningCounter;
            } else if (iter->value("message", "") == "Invalid value 'image/webp'. Valid values are ('image/jpeg', 'image/png').") {
                iter = messagesArray.erase(iter);
                --warningCounter;
            } else if (iter->value("severity", 0) == 3) {
                iter = messagesArray.erase(iter);
            } else {
                ++iter;
            }
        }

        input.at("/issues/numWarnings"_json_pointer) = warningCounter;
        input.at("/issues/numHints"_json_pointer) = 0;
    }
}
