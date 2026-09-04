#pragma once

#include "../Protocol/Protocol.h"

/**
 * NFC の 1 回の転送を管理する。
 *
 * フレームを受け取って応答を返すだけで、通信も保存も画面も触らない。
 * 呼び出し側から受け皿のメモリを渡してもらうので、
 * 実機に依存せず native 環境で単体テストできる。
 *
 * 途中で通信が切れても、同じ内容で BEGIN をやり直せば
 * 受け取り済みの位置から続けられる。NFC はスマホを離すと切れるため、
 * やり直しが要る前提で組んでいる。
 */
class NfcSession
{
public:
    /// 受け皿を渡して使えるようにする
    void begin(uint8_t *buffer, size_t capacity, int imageWidth, int imageHeight);

    /**
     * 受け取ったフレームを処理して応答を組み立て、その長さを返す。
     * 応答を返せない場合は 0。
     */
    size_t handle(const uint8_t *request, size_t requestSize, uint8_t *response, size_t responseCapacity);

    /// 画像を最後まで受け取って検査も通ったか
    bool isComplete() const { return _complete; }

    const uint8_t *image() const { return _buffer; }
    size_t imageSize() const { return _totalBytes; }

    /// 受け取り済みの大きさ（途中経過）
    uint32_t receivedBytes() const { return _receivedBytes; }

    /// 転送をやめて最初の状態に戻す
    void reset();

private:
    uint8_t *_buffer = nullptr;
    size_t _capacity = 0;
    int _imageWidth = 0;
    int _imageHeight = 0;

    uint32_t _transferId = 0;
    uint32_t _totalBytes = 0;
    uint32_t _expectedCrc = 0;
    uint32_t _receivedBytes = 0;
    bool _complete = false;

    size_t handleHello(uint8_t *response, size_t capacity);
    size_t handleBegin(const NfcProtocol::Request &request, uint8_t *response, size_t capacity);
    size_t handleData(const NfcProtocol::Request &request, uint8_t *response, size_t capacity);
    size_t handleCommit(const NfcProtocol::Request &request, uint8_t *response, size_t capacity);
    size_t handleAbort(const NfcProtocol::Request &request, uint8_t *response, size_t capacity);
};
