#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <DeviceProfile.h>

/**
 * NFC で画像を受け取る。
 *
 * 本体をタグとして振る舞わせ、スマホのアプリから画像を送ってもらう。
 * ブラウザからは扱えない。iOS には NFC の API が無く、Android の
 * Web NFC も NDEF しか扱えないため、専用のアプリが要る。
 *
 * NFC を積むのは M5PaperMono だけなので、この機能もその機種に限る。
 */
namespace NfcTransfer
{
#if defined(ARDUINO) && defined(CARDCASE_HAS_NFC)
    /// 受け取れる状態にする。使えない機種や初期化に失敗した場合は false。
    bool begin(const DeviceProfile &profile);

    /// loop() から呼ぶ
    void update();

    /**
     * 画像を受け取るかどうか。
     *
     * 待ち受けは止められない（止めると次から届かなくなる）ので、
     * 受け取りたくない画面では断るようにする。
     */
    void setAccepting(bool accepting);

    /// 停止して電波を止める
    void end();

    /// 画像を受け取ったか
    bool hasReceivedImage();

    /// 受け取った画像。begin() から end() までの間だけ有効。
    const uint8_t *receivedImage(size_t &size);

    /// SD に保存できた場合のパス。保存していなければ空。
    String receivedImagePath();

    /// 受け取った画像を捨てて、次を待つ
    void releaseReceivedImage();

    /// 受け取り済みの大きさ。進み具合の表示に使う。
    uint32_t receivedBytes();

    /// 送られてくる画像全体の大きさ。まだ分からなければ 0。
    uint32_t totalBytes();
#endif
}
