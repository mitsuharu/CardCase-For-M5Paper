#include "./M5Helper.h"

#ifdef ARDUINO

#include <M5Unified.h>
#include <Storage.h>
#include <ExifOrientation.h>
#include <ImageSize.h>
#include <ImageRotation.h>

namespace
{
    // 拡大率 0.0f を渡すと画面に収まる倍率が自動で計算される
    const float kAutoFit = 0.0f;

    uint16_t convertColor(M5Helper::Color color)
    {
        if (color == M5Helper::Color::black)
        {
            return TFT_BLACK;
        }
        else
        {
            return TFT_WHITE;
        }
    }

    bool endsWithIgnoreCase(const String &value, const char *suffix)
    {
        String lowered = value;
        lowered.toLowerCase();
        return lowered.endsWith(suffix);
    }

    /// 画像の先頭バイト列から、表示するときの画面の回転を決める
    int displayRotationForHeader(const uint8_t *data, size_t size, const DeviceProfile &profile)
    {
        int orientation = ImageFile::exifOrientation(data, size);

        int imageWidth = 0;
        int imageHeight = 0;
        if (!ImageFile::imageSize(data, size, &imageWidth, &imageHeight))
        {
            imageWidth = 0;
            imageHeight = 0;
        }

        int fitSteps = (profile.imageFitRotation == FitRotation::Clockwise)
                           ? ImageFile::kFitStepsClockwise
                           : ImageFile::kFitStepsCounterClockwise;

        return ImageFile::displayRotation(orientation, imageWidth, imageHeight,
                                          static_cast<int>(profile.imageRotation),
                                          profile.width, profile.height, fitSteps);
    }

    /// PNG のシグネチャを持つか
    bool isPng(const uint8_t *data, size_t size)
    {
        static const uint8_t kSignature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        if (size < sizeof(kSignature))
        {
            return false;
        }
        for (size_t i = 0; i < sizeof(kSignature); i++)
        {
            if (data[i] != kSignature[i])
            {
                return false;
            }
        }
        return true;
    }

    /// 描画前の共通の下ごしらえ。回転と描画モードを決めて画面を白で埋める。
    void beginImageFrame(int rotation)
    {
        M5.Display.setRotation(static_cast<uint_fast8_t>(rotation));
        if (M5.Display.isEPD())
        {
            // 表示する画像そのものなので画質を優先する。
            // 点滅の少ない波形もあるが、前の画面が残るので使わない。
            M5.Display.setEpdMode(epd_mode_t::epd_quality);
        }

        M5.Display.startWrite();
        M5.Display.fillScreen(TFT_WHITE);
    }

    /**
     * ファイルから、表示するときの画面の回転を決める。
     * ここはファイルを読む部分だけを持つ。
     */
    int displayRotationFor(const String &path, const DeviceProfile &profile)
    {
        // EXIF にサムネイルが入っていると SOF は数十 KB 先になるので広めに読む
        const size_t kHeaderSize = 64 * 1024;

        File file = Storage::fs().open(path.c_str(), FILE_READ);
        if (!file)
        {
            return static_cast<int>(profile.imageRotation);
        }

        size_t size = file.size();
        if (size > kHeaderSize)
        {
            size = kHeaderSize;
        }

        // 数十 KB になるので PSRAM を優先する
        uint8_t *buffer = static_cast<uint8_t *>(ps_malloc(size));
        if (buffer == nullptr)
        {
            buffer = static_cast<uint8_t *>(malloc(size));
        }
        if (buffer == nullptr)
        {
            file.close();
            return static_cast<int>(profile.imageRotation);
        }

        size_t read = file.read(buffer, size);
        file.close();

        int rotation = displayRotationForHeader(buffer, read, profile);
        free(buffer);
        return rotation;
    }
}

// SDカード内の画像ファイルを描画する関数
void M5Helper::drawImageFromSD(const String &path, const DeviceProfile &profile)
{
    bool jpeg = endsWithIgnoreCase(path, ".jpg") || endsWithIgnoreCase(path, ".jpeg");

    // drawJpg は EXIF も画面との向きも見ないので、画面側を回して辻褄を合わせる。
    // 回転の判断でも SD を読むため、画面のトランザクションより先に済ませる。
    int rotation = displayRotationFor(path, profile);

    // ファイルを開くのも画面のトランザクションに入る前。
    //
    // M5Paper は EPD（IT8951）と SD が SPI を共有しており、startWrite() で
    // バスを保持したまま SD を操作すると固まる。デコード中の読み出しは
    // LGFX が need_transaction を見てバスを解放してくれるが、
    // 自分で呼ぶ open / close はその対象外なので外に出す必要がある。
    //
    // ファイルを自分で開いて渡すのは、パスとファイルシステムを渡す API が
    // 具体的な型（SDFS / SDMMCFS）に対するテンプレートで、
    // 機種ごとに違うファイルシステムを扱うこちらの作りには合わないため。
    File file = Storage::fs().open(path.c_str(), FILE_READ);

    beginImageFrame(rotation);

    if (file)
    {
        if (jpeg)
        {
            M5.Display.drawJpg(&file, 0, 0, 0, 0, 0, 0, kAutoFit, kAutoFit, datum_t::middle_center);
        }
        else
        {
            M5.Display.drawPng(&file, 0, 0, 0, 0, 0, 0, kAutoFit, kAutoFit, datum_t::middle_center);
        }
    }

    M5.Display.endWrite();

    // 閉じるのも SD の操作なのでトランザクションの外で行う
    if (file)
    {
        file.close();
    }

    // M5PaperColor は 1 画面のリフレッシュに 15〜30 秒かかる。
    // 待たずにスリープすると描画が途中で切れるため、必ず完了を待つ。
    M5.Display.waitDisplay();
}

void M5Helper::drawImageFromMemory(const uint8_t *data, size_t size, const DeviceProfile &profile)
{
    if (data == nullptr || size == 0)
    {
        return;
    }

    beginImageFrame(displayRotationForHeader(data, size, profile));

    if (isPng(data, size))
    {
        M5.Display.drawPng(data, size, 0, 0, 0, 0, 0, 0, kAutoFit, kAutoFit, datum_t::middle_center);
    }
    else
    {
        M5.Display.drawJpg(data, size, 0, 0, 0, 0, 0, 0, kAutoFit, kAutoFit, datum_t::middle_center);
    }

    M5.Display.endWrite();
    M5.Display.waitDisplay();
}

M5Helper::Size M5Helper::drawText(const String &text, int x, int y, int textSize, Color fontColor, Color bgColor)
{
    M5.Display.setTextSize(textSize);
    M5.Display.setTextColor(convertColor(fontColor), convertColor(bgColor));
    M5.Display.setCursor(x, y);
    M5.Display.setTextWrap(false);
    if (M5.Display.isEPD())
    {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    }
    M5.Display.println(text.c_str());
    M5.Display.setTextWrap(true);

    int width = M5.Display.width();
    int height = M5.Display.fontHeight();

    M5Helper::Size size = {width, height};
    return size;
}

#endif
