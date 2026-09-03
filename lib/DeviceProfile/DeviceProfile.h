#pragma once

/**
 * 機種ごとの差分をここ 1 箇所に集約する。
 *
 * DeviceKind と profileFor() は Arduino に依存しない純粋なロジックなので、
 * native 環境の単体テストから直接呼べる。実機の判定（M5.getBoard() の変換）だけを
 * ARDUINO 側のアダプタに閉じ込めている。
 */

#ifdef ARDUINO
#include <Arduino.h>
#endif

/// 対応する M5 電子ペーパー機種
enum class DeviceKind
{
    Unknown,
    M5Paper,    // M5Paper v1.1 (ESP32)
    PaperS3,    // M5PaperS3
    PaperColor, // M5PaperColor
    PaperMono,  // M5PaperMono
};

/// 画面の回転。M5GFX の setRotation() に渡す値と対応する。
enum class DeviceRotation
{
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3
};

/// 画面が表現できる色
enum class DevicePalette
{
    Gray16, // 16 階調グレースケール
    Gray4,  // 4 階調グレースケール
    Color6, // 6 色（E Ink Spectra 6）
};

struct DeviceProfile
{
    DeviceKind kind = DeviceKind::Unknown;

    /// 起動バナーや README に出す表示名
    const char *name = "Unknown";

    /// 回転 0 のときの画面サイズ
    int width = 0;
    int height = 0;

    /**
     * 画像を全画面表示するときの回転。
     * M5PaperS3 は筐体上部にフックがあり、掛けると上下が逆になるため Down にする。
     * フックのない他機種は Up のまま。
     */
    DeviceRotation imageRotation = DeviceRotation::Up;

    /// タッチパネルの有無。M5PaperColor だけ非搭載でボタン操作しかできない。
    bool hasTouch = false;

    /// BtnA/B/C として使える物理ボタンの数（電源ボタンは含まない）
    int buttonCount = 0;

    DevicePalette palette = DevicePalette::Gray16;

    /// フロントライトの有無（M5PaperMono のみ）
    bool hasFrontlight = false;

    /// NFC リーダーの有無（M5PaperMono のみ）
    bool hasNfc = false;

    /// 一覧のファイル名を描画するときの文字サイズ
    int menuTextSize = 8;

    bool isColor() const { return palette == DevicePalette::Color6; }

    /// タッチもボタンも無い＝操作不能。ビルド設定の取り違えを検出するために使う。
    bool isOperable() const { return hasTouch || buttonCount > 0; }
};

/// 機種から固定のプロファイルを引く。副作用のない純粋関数。
DeviceProfile profileFor(DeviceKind kind);

#ifdef ARDUINO
/// M5Unified の自動判定結果を DeviceKind に写す
DeviceKind currentDeviceKind();

/// 実機のプロファイル
DeviceProfile currentProfile();
#endif
