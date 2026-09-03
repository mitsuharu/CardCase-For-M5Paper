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

/**
 * 物理ボタンに割り当てる役割。
 *
 * 同じ BtnA でも機種によって物理的な位置が違う。
 * M5Paper v1.1 はロータリスイッチの 上 / 押込 / 下 が A / B / C に並ぶが、
 * M5PaperColor は上下ボタンが A / B に隣り合い、3 つ目が C になる。
 * どのボタンが何をするかは機種ごとに決める。
 */
enum class ButtonRole
{
    None,
    Prev,   // 選択を 1 つ上へ
    Next,   // 選択を 1 つ下へ
    Select, // 決定
};

/**
 * 画像を出すときの電子ペーパーの更新の仕方。
 *
 * 階調を出す波形は白黒の反転を何度も繰り返すため、画面が派手に点滅する。
 * 階調の幅が狭い機種では、そこまでの手間をかける価値がない。
 */
enum class RefreshMode
{
    Quality, // 階調を優先する。反転を伴うので点滅が目立つ
    Fast,    // 点滅を抑える。多少の残像は許容する
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

    /// BtnA / BtnB / BtnC の役割（電源ボタンは含まない）
    ButtonRole buttonA = ButtonRole::None;
    ButtonRole buttonB = ButtonRole::None;
    ButtonRole buttonC = ButtonRole::None;

    DevicePalette palette = DevicePalette::Gray16;

    /// 画像を表示するときの更新の仕方
    RefreshMode imageRefresh = RefreshMode::Quality;

    /// フロントライトの有無（M5PaperMono のみ）
    bool hasFrontlight = false;

    /// NFC リーダーの有無（M5PaperMono のみ）
    bool hasNfc = false;

    /**
     * 1 回の画面更新に十数秒かかる機種か（M5PaperColor）。
     * この機種のパネルは部分更新を持たず、更新のたびに全面を転送するため、
     * 描画をまとめるだけでなく、更新の回数そのものを減らす必要がある。
     */
    bool slowRefresh = false;

    /// 一覧のファイル名を描画するときの文字サイズ
    int menuTextSize = 8;

    bool isColor() const { return palette == DevicePalette::Color6; }

    /// ボタンで操作できるか
    bool hasButtons() const
    {
        return buttonA != ButtonRole::None || buttonB != ButtonRole::None || buttonC != ButtonRole::None;
    }

    /// タッチもボタンも無い＝操作不能。ビルド設定の取り違えを検出するために使う。
    bool isOperable() const { return hasTouch || hasButtons(); }
};

/// 機種から固定のプロファイルを引く。副作用のない純粋関数。
DeviceProfile profileFor(DeviceKind kind);

#ifdef ARDUINO
/// M5Unified の自動判定結果を DeviceKind に写す
DeviceKind currentDeviceKind();

/// 実機のプロファイル
DeviceProfile currentProfile();
#endif
