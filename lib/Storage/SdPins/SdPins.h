#pragma once

#include <stdint.h>

/**
 * microSD の配線。
 *
 * 値は M5Unified のピンテーブルから 1 本ずつ受け取る。
 * 「D1 は D0 の隣だろう」といった推測で番号を作らないこと。
 * SPI の機種では D1 / D2 が未接続（-1）で、その番号は別の用途に使われている。
 *
 * 判定だけの純粋なロジックなので native 環境で単体テストする。
 */
struct SdPins
{
    static const int kMaxPullUpPins = 5;

    int8_t clk = -1;
    int8_t cmd = -1; // SPI では MOSI
    int8_t d0 = -1;  // SPI では MISO
    int8_t d1 = -1;  // SPI では未接続
    int8_t d2 = -1;  // SPI では未接続
    int8_t d3 = -1;  // SPI では CS

    /// 読み書きに必要なピンが揃っているか
    bool isValid() const;

    /// 4bit の SDMMC で配線されているか。D1 / D2 が出ていればそちら。
    bool usesSdmmc() const;

    /**
     * 内部プルアップにするピンを out に詰めて、その個数を返す。
     *
     * SDMMC のときだけ意味を持つ。Arduino の SD_MMC はスロットの flags を 0 の
     * ままにしていて内部プルアップを有効にしないため、外付けのプルアップが無い
     * 配線ではラインが浮いてカードが応答しない。
     *
     * クロックは対象にしない。また、ここが返すのは必ずこの構造体が持っている
     * ピンのいずれかで、計算で作った番号を返すことはない。
     */
    int pullUpPins(int8_t *out, int capacity) const;
};
