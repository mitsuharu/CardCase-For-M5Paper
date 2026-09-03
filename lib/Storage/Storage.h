#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <FS.h>
#endif

/**
 * microSD へのアクセス。
 *
 * つなぎ方が機種によって違う（SPI か 4bit の SDMMC か）ので、
 * 呼び出し側が意識しなくて済むようにここへ閉じ込める。
 */
namespace Storage
{
#ifdef ARDUINO
    /**
     * マウントする。復帰直後などは一度で成功しないことがあるのでやり直す。
     * SPI か SDMMC かは M5Unified のピンテーブルから判断する。
     */
    bool begin();

    /**
     * アンマウントする。
     *
     * ディープスリープ中も SD カードには通電したままなので、読み出しの途中で寝ると
     * カードが中途半端な状態で残り、次回のマウントが失敗する。寝る前に必ず呼ぶ。
     */
    void end();

    /// 画像を読むためのファイルシステム。begin() が成功したあとに使う。
    fs::FS &fs();
#endif
}
