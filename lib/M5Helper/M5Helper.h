#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
using String = std::string;
#endif

#include <DeviceProfile.h>

struct M5Helper
{
    enum class Color
    {
        white,
        black
    };

    struct Size
    {
        int width;
        int height;
    };

    /**
     * SD カード内の画像ファイルを全画面に描画する。
     *
     * 回転はプロファイル任せ（M5PaperS3 だけフックに合わせて上下反転する）。
     * 画像は画面に収まるよう自動で縮小し、中央に寄せるので、
     * 画面サイズちょうどでない画像や機種違いの画像でも破綻しない。
     *
     * 描画の完了を待ってから戻る。
     */
    static void drawImageFromSD(const String &path, const DeviceProfile &profile);

    static Size drawText(const String &text, int x, int y, int textSize = 6, Color fontColor = Color::black, Color bgColor = Color::white);
};
