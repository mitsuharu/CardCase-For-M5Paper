#include "./M5Helper.h"

#ifdef ARDUINO

#include <M5Unified.h>
#include <Storage.h>
#include <ExifOrientation.h>
#include <ImageSize.h>
#include <ImageRotation.h>

namespace
{
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

    /**
     * 画像を表示するときの画面の回転を決める。
     *
     * EXIF の向きとピクセル寸法を読み、あとの判断は displayRotation に任せる。
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

        int orientation = ImageFile::exifOrientation(buffer, read);

        int imageWidth = 0;
        int imageHeight = 0;
        if (!ImageFile::imageSize(buffer, read, &imageWidth, &imageHeight))
        {
            imageWidth = 0;
            imageHeight = 0;
        }
        free(buffer);

        int fitSteps = (profile.imageFitRotation == FitRotation::Clockwise)
                           ? ImageFile::kFitStepsClockwise
                           : ImageFile::kFitStepsCounterClockwise;

        return ImageFile::displayRotation(orientation, imageWidth, imageHeight,
                                          static_cast<int>(profile.imageRotation),
                                          profile.width, profile.height, fitSteps);
    }
}

// SDカード内の画像ファイルを描画する関数
void M5Helper::drawImageFromSD(const String &path, const DeviceProfile &profile)
{
    bool isJpeg = endsWithIgnoreCase(path, ".jpg") || endsWithIgnoreCase(path, ".jpeg");

    // drawJpgFile は EXIF も画面との向きも見ないので、画面側を回して辻褄を合わせる
    int rotation = displayRotationFor(path, profile);

    // 回転と描画モードは fillScreen より先に決める。
    // epd_fastest のまま塗り潰すと、LGFX が endWrite の時点で高速波形のまま
    // 画面を更新してしまい、黒が白に戻りきらずメニューの残像が余白に残る。
    M5.Display.setRotation(static_cast<uint_fast8_t>(rotation));
    if (M5.Display.isEPD())
    {
        // 表示する画像そのものなので画質を優先する。
        // 点滅の少ない波形もあるが、前の画面が残るので使わない。
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
    }

    // 塗り潰しと画像の描画を 1 回の更新にまとめる。
    // 電子ペーパーは更新が遅いので、途中で走らせない。
    M5.Display.startWrite();

    M5.Display.fillScreen(TFT_WHITE);

    // 拡大率 0.0f を渡すと画面に収まる倍率が自動で計算される。
    // datum に middle_center を指定して余白を上下左右に均等に振る。
    //
    // ファイルは自分で開いて渡す。パスとファイルシステムを渡す API は
    // 具体的な型（SDFS / SDMMCFS）に対するテンプレートなので、
    // 機種ごとに違うファイルシステムを扱うこちらの作りには合わない。
    const float autoFit = 0.0f;
    File file = Storage::fs().open(path.c_str(), FILE_READ);
    if (file)
    {
        if (isJpeg)
        {
            M5.Display.drawJpg(&file, 0, 0, 0, 0, 0, 0, autoFit, autoFit, datum_t::middle_center);
        }
        else if (endsWithIgnoreCase(path, ".png"))
        {
            M5.Display.drawPng(&file, 0, 0, 0, 0, 0, 0, autoFit, autoFit, datum_t::middle_center);
        }
        file.close();
    }

    M5.Display.endWrite();

    // M5PaperColor は 1 画面のリフレッシュに 15〜30 秒かかる。
    // 待たずにスリープすると描画が途中で切れるため、必ず完了を待つ。
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
