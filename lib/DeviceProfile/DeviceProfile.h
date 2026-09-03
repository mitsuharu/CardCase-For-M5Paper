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
 * 画面の向きに合わせて画像を回すときの向き。
 *
 * どちらに回しても画面には収まるので、見るときに本体をどちらへ傾けるかで決める。
 * M5PaperS3 はフックに合わせて画像全体を 180 度回しているため、
 * 傾ける向きも他機種と逆になる。
 */
enum class FitRotation
{
    CounterClockwise,
    Clockwise,
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

    /// 横長の画像を画面の向きに合わせるときに回す向き
    FitRotation imageFitRotation = FitRotation::CounterClockwise;

    /// タッチパネルの有無。M5PaperColor だけ非搭載でボタン操作しかできない。
    bool hasTouch = false;

    /// BtnA / BtnB / BtnC の役割（電源ボタンは含まない）
    ButtonRole buttonA = ButtonRole::None;
    ButtonRole buttonB = ButtonRole::None;
    ButtonRole buttonC = ButtonRole::None;

    DevicePalette palette = DevicePalette::Gray16;

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

    /**
     * 画面の端に取る余白。
     *
     * 筐体のベゼルが画面の縁に掛かるため、端まで描くと文字が読めなくなる。
     * 画面の大きさに対する割合で決める。固定値だと、幅の狭い機種
     * （M5PaperColor は 400px）で相対的に足りなくなる。
     */
    int margin() const
    {
        int value = width / 25;
        return (value < 8) ? 8 : value;
    }

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
