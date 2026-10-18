#pragma once

#include <QQuickImageProvider>

namespace ux3d::editor {

/**
 * @brief enum for image types
 * calles "EImageType" to avoid ambiguity with "ImageType" from Qt
 *
 */
enum EImageType {
    PNGORJPEG,
    KTX,
    WEBP
};
class ImageProvider : public QQuickImageProvider {
    Q_OBJECT
public:
    ImageProvider();
    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;

    std::map<std::string, QPixmap> m_imageData;

private:
    void addPixmap(const std::vector<uint8_t>& data, const std::string& id, EImageType imageType);
};
}
