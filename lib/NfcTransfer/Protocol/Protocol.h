#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * NFC で画像を受け取るときのやり取りの決まり。
 *
 * スマホを「読み取る側」、本体を「タグ」として振る舞わせ、
 * NFC-A の生のフレームで独自のコマンドをやり取りする。
 * NDEF は使わない。NDEF は小さなテキストや URL を置く用途で、
 * 数十 KB の画像を分割して送るのには向かないため。
 *
 * バイト列の読み書きだけの純粋なロジックなので native 環境で単体テストする。
 */
namespace NfcProtocol
{
    /// 先頭に置く印。CardCase の頭文字。
    const uint8_t kMagic0 = 'C';
    const uint8_t kMagic1 = 'C';

    const uint8_t kVersion = 1;

    /**
     * 1 フレームの最大の長さ。
     *
     * ISO14443-A のフレームは CRC を除いて 253 バイトまで。
     * これを超えると RF の層で弾かれる。
     */
    const size_t kMaxFrameSize = 253;

    /// 要求と応答に共通する先頭部分（印 2 + 版 1 + コマンド 1）
    const size_t kHeaderSize = 4;

    /// 応答は先頭部分に状態を 1 バイト足す
    const size_t kResponseHeaderSize = kHeaderSize + 1;

    /// DATA で 1 回に送れる本体の最大。フレームの上限から見出しを引いた値。
    const size_t kMaxChunkSize = kMaxFrameSize - (kHeaderSize + 4 + 4);

    /// 受け取れる画像の最大の大きさ
    const uint32_t kMaxImageSize = 256u * 1024u;

    enum Command : uint8_t
    {
        CommandHello = 0x01,  // 能力の問い合わせ
        CommandBegin = 0x02,  // 転送の開始。大きさと CRC を伝える
        CommandData = 0x03,   // 本体を分割して送る
        CommandCommit = 0x04, // 転送の確定
        CommandAbort = 0x05,  // 転送の取り消し
    };

    enum Status : uint8_t
    {
        StatusOk = 0x00,
        StatusAccepted = 0x01,

        StatusBadRequest = 0x10,  // 形式が違う、知らないコマンド、版が違う
        StatusBadOffset = 0x11,   // 続きの位置が合わない
        StatusTooLarge = 0x12,    // 受け取れる大きさを超えている
        StatusNoTransfer = 0x13,  // その転送を知らない
        StatusCrcMismatch = 0x14, // 中身が壊れている
        StatusIncomplete = 0x15,  // まだ全部届いていない
    };

    /// 受け取ったフレームを読み解いた結果
    struct Request
    {
        uint8_t command = 0;
        uint32_t transferId = 0;
        uint32_t offset = 0;
        uint32_t totalBytes = 0;
        uint32_t crc32 = 0;
        const uint8_t *payload = nullptr;
        size_t payloadSize = 0;
    };

    /// 大きい方が先の並びで 32bit を読む
    uint32_t readU32(const uint8_t *data);

    /// 大きい方が先の並びで 32bit を書く
    void writeU32(uint8_t *data, uint32_t value);

    /**
     * フレームを読み解く。
     * 印や版が違う、長さが足りない場合は false を返す。
     */
    bool parseRequest(const uint8_t *data, size_t size, Request &out);

    /**
     * 応答のフレームを組み立てて、その長さを返す。
     * payload は状態のうしろに続く。入らない場合は 0 を返す。
     */
    size_t buildResponse(uint8_t command, uint8_t status,
                         const uint8_t *payload, size_t payloadSize,
                         uint8_t *out, size_t outCapacity);

    /// 画像の壊れを見つけるための検査値（IEEE 802.3 の CRC-32）
    uint32_t crc32(const uint8_t *data, size_t size, uint32_t seed = 0);
}
