#include "ImageProvider.h"
#include "model/ProjectManager.h"
#include "reactor/ImageOperations.h"
#include "reactor/Json.h"
#include "reactor/Utils.h"
#include <QIcon>

using namespace ux3d::editor;

ImageProvider::ImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap, QQmlImageProviderBase::ForceAsynchronousImageLoading)
{
    model::ProjectManager::getInstance()->model.binDiffComb.observe_on(rxcpp::observe_on_new_thread()).subscribe([this](const std::tuple<nlohmann::json, nlohmann::json>& tuple) {
        const nlohmann::json patch = std::get<0>(tuple);
        const nlohmann::json state = std::get<1>(tuple);
        if (!state.contains("/gltf/images"_json_pointer)) {
            return;
        }
        for (auto& change : patch) {
            if (change.at("op") == "remove") {
                // Remove old data?
            } else {
                if (!change.at("value").is_number()) {
                    // Should not happen
                    continue;
                }
                nlohmann::json::json_pointer path(change.at("path"));
                std::string pathPrefix = change.at("path").get<std::string>().substr(0, reactor::json::findNth(change.at("path"), "/", 2, 0));
                if (pathPrefix == "/glb") {
                    // Find all images in buffer
                    for (auto& image : state.at("/gltf/images"_json_pointer)) {
                        if (!image.contains("uri") && image.contains("bufferView") && image.contains("mimeType")) {
                            int bufferView = image.at("bufferView");
                            nlohmann::json::json_pointer bufferViewPoiner = "/gltf/bufferViews"_json_pointer / bufferView;
                            int buffer = state.value(bufferViewPoiner / "buffer", -1);
                            if (buffer == 0 && !state.contains("/gltf/buffers"_json_pointer / buffer / "uri")) {
                                const auto& datastorage = model::ProjectManager::getInstance()->model.m_dataStorage;
                                const std::string id = std::to_string(change.at("value").get<int>()) + bufferViewPoiner.to_string();
                                try {
                                    const auto& imageData = reactor::utils::getBufferViewData(bufferViewPoiner, state, datastorage);
                                    if (image.at("mimeType") == "image/ktx2") {
                                        addPixmap(imageData, id, EImageType::KTX);
                                    } else if (image.at("mimeType") == "image/webp") {
                                        addPixmap(imageData, id, EImageType::WEBP);
                                    } else {
                                        addPixmap(imageData, id, EImageType::PNGORJPEG);
                                    }
                                } catch (...) {
                                    m_imageData.insert_or_assign(id, QPixmap());
                                }
                            }
                        }
                    }
                } else if (pathPrefix == "/files") {
                    const size_t lastIndexOfPeriod = path.back().find_last_of(".");
                    if (lastIndexOfPeriod == std::string::npos) {
                        // image has no file extension
                        continue;
                    }
                    std::string fileEnding = path.back().substr(lastIndexOfPeriod);
                    if (fileEnding == ".bin") {
                        for (auto& image : state.at("/gltf/images"_json_pointer)) {
                            if (!image.contains("uri") && image.contains("bufferView") && image.contains("mimeType")) {
                                int bufferView = image.at("bufferView");
                                nlohmann::json::json_pointer bufferViewPoiner = "/gltf/bufferViews"_json_pointer / bufferView;
                                int buffer = state.value(bufferViewPoiner / "buffer", -1);
                                if (buffer != -1 && state.contains("/gltf/buffers"_json_pointer / buffer / "uri") && state.at("/gltf/buffers"_json_pointer / buffer / "uri") == path.back()) {
                                    const auto& datastorage = model::ProjectManager::getInstance()->model.m_dataStorage;
                                    const std::string id = std::to_string(change.at("value").get<int>()) + bufferViewPoiner.to_string();
                                    try {
                                        const auto& imageData = reactor::utils::getBufferViewData(bufferViewPoiner, state, datastorage);
                                        if (image.at("mimeType") == "image/ktx2") {
                                            addPixmap(imageData, id, EImageType::KTX);
                                        } else if (image.at("mimeType") == "image/webp") {
                                            addPixmap(imageData, id, EImageType::WEBP);
                                        } else {
                                            addPixmap(imageData, id, EImageType::PNGORJPEG);
                                        }
                                    } catch (...) {
                                        m_imageData.insert_or_assign(id, QPixmap());
                                    }
                                }
                            }
                        }
                    } else if (fileEnding == ".png" || fileEnding == ".jpeg" || fileEnding == ".jpg" || fileEnding == ".webp" || fileEnding == ".ktx" || fileEnding == ".ktx2") {
                        const std::string id = std::to_string(change.at("value").get<int>()) + path.back();
                        if (!model::ProjectManager::getInstance()->model.m_dataStorage.contains(change.at("value"))) {
                            m_imageData.insert_or_assign(id, QPixmap());
                            continue;
                        }
                        const auto& imageData = model::ProjectManager::getInstance()->model.m_dataStorage.at(change.at("value"));

                        EImageType imageType = EImageType::PNGORJPEG;
                        if(fileEnding == ".webp") {
                            imageType = EImageType::WEBP;
                        } else if(fileEnding == ".ktx" || fileEnding == ".ktx2") {
                            imageType = EImageType::KTX;
                        }

                        addPixmap(imageData, id, imageType);
                    }
                } else if (pathPrefix == "/embedded") {
                    //Check if this is used in an image
                    for (auto& image : state.at("/gltf/images"_json_pointer)) {
                        if (image.contains("uri") && image.at("uri") == path.back()) {
                            const auto& imageData = model::ProjectManager::getInstance()->model.m_dataStorage.at(change.at("value"));
                            const std::string id = std::to_string(change.at("value").get<int>()) + path.back();
                            if (!model::ProjectManager::getInstance()->model.m_dataStorage.contains(change.at("value"))) {
                                m_imageData.insert_or_assign(id, QPixmap());
                                break;
                            }

                            if(image.value("mimeType", "") == "image/ktx2") {
                                addPixmap(imageData, id, EImageType::KTX);
                            }
                            else if(image.value("mimeType", "") == "image/webp") {
                                addPixmap(imageData, id, EImageType::WEBP);
                            } else {
                                addPixmap(imageData, id, EImageType::PNGORJPEG);
                            }
                            break;
                        }
                    }
                }
            }
        } }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
}

void ImageProvider::addPixmap(const std::vector<uint8_t>& data, const std::string& id, EImageType imageType)
{
    QPixmap pixmap;
    if (imageType == EImageType::KTX || imageType == EImageType::WEBP) {
        std::vector<uint8_t> pngData;
        try {
            auto decodedImage = reactor::image::decode(data);
            pngData = reactor::image::encode(decodedImage, "{\"mimeType\": \"image/png\"}"_json);
        } catch (std::exception& e) {
            std::string error = e.what();
        }
        pixmap.loadFromData(pngData.data(), pngData.size());
    } else {
        pixmap.loadFromData(data.data(), data.size());
    }
    m_imageData.insert_or_assign(id, pixmap);
}

QPixmap ImageProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize)
{
    if (!m_imageData.contains(id.toStdString())) {
        return QPixmap();
    }

    const QPixmap& pixmap = m_imageData.at(id.toStdString());

    if (pixmap.isNull()) {
        QSize actualSize = QSize(requestedSize.width() / 2, requestedSize.height() / 2);
        if (actualSize.width() == 0) {
            actualSize.setWidth(actualSize.height());
        }
        if (actualSize.height() == 0) {
            actualSize.setHeight(actualSize.width());
        }
        if (actualSize.height() == 0) {
            actualSize = QSize(256, 256);
        }
        QPixmap errorPixmap = QIcon(":/icons/image_cant_be_displayed.svg").pixmap(actualSize).scaled(actualSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (size)
            *size = QSize(errorPixmap.width(), errorPixmap.height());
        return errorPixmap;
    }

    int width = pixmap.width();
    int height = pixmap.height();
    if (size)
        *size = QSize(width, height);
    bool scale = false;
    if (requestedSize.width() > 0 && requestedSize.width() != width) {
        scale = true;
        width = requestedSize.width();
    }
    if (requestedSize.height() > 0 && requestedSize.height() != height) {
        height = requestedSize.height();
        scale = true;
    }
    if (scale) {
        return pixmap.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return pixmap;
}
