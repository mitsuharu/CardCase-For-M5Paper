#include "./M5Helper.h"

#ifdef ARDUINO

#include <SD.h>
#include <M5Unified.h>

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
}

// SDカード内の画像ファイルを描画する関数
void M5Helper::drawImageFromSD(const String &path, const DeviceProfile &profile)
{
    // 回転と描画モードは fillScreen より先に決める。
    // epd_fastest のまま塗り潰すと、LGFX が endWrite の時点で高速波形のまま
    // 画面を更新してしまい、黒が白に戻りきらずメニューの残像が余白に残る。
    M5.Display.setRotation(static_cast<uint_fast8_t>(profile.imageRotation));
    if (M5.Display.isEPD())
    {
        // 表示する画像そのものなので画質を優先する
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
    }

    // 塗り潰しと画像の描画を 1 回の更新にまとめる。
    // 電子ペーパーは更新が遅いので、途中で走らせない。
    M5.Display.startWrite();

    M5.Display.fillScreen(TFT_WHITE);

    // 拡大率 0.0f を渡すと画面に収まる倍率が自動で計算される。
    // datum に middle_center を指定して余白を上下左右に均等に振る。
    const float autoFit = 0.0f;
    if (endsWithIgnoreCase(path, ".jpg") || endsWithIgnoreCase(path, ".jpeg"))
    {
        M5.Display.drawJpgFile(SD, path.c_str(), 0, 0, 0, 0, 0, 0, autoFit, autoFit, datum_t::middle_center);
    }
    else if (endsWithIgnoreCase(path, ".png"))
    {
        M5.Display.drawPngFile(SD, path.c_str(), 0, 0, 0, 0, 0, 0, autoFit, autoFit, datum_t::middle_center);
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
