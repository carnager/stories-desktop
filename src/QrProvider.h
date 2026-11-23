#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QUrl>
#include <qrencode.h>

/** image://qr/<percent-encoded text> as a QR code with its quiet zone. */
class QrProvider : public QQuickImageProvider {
public:
    QrProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        const QByteArray text = QUrl::fromPercentEncoding(id.toLatin1()).toUtf8();
        QRcode *code = QRcode_encodeString(text.constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
        if (!code)
            return {};
        const int quiet = 4, scale = 8, side = (code->width + 2 * quiet) * scale;
        QImage image(side, side, QImage::Format_RGB32);
        image.fill(Qt::white);
        for (int y = 0; y < code->width; ++y) {
            for (int x = 0; x < code->width; ++x) {
                if (!(code->data[y * code->width + x] & 1))
                    continue;
                for (int py = 0; py < scale; ++py)
                    for (int px = 0; px < scale; ++px)
                        image.setPixel((x + quiet) * scale + px, (y + quiet) * scale + py, 0xff000000);
            }
        }
        QRcode_free(code);
        if (size)
            *size = image.size();
        return image;
    }
};
