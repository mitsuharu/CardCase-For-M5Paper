#include "Session.h"

using namespace NfcProtocol;

namespace
{
    void writeU16(uint8_t *data, uint16_t value)
    {
        data[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[1] = static_cast<uint8_t>(value & 0xFF);
    }
}

void NfcSession::begin(uint8_t *buffer, size_t capacity, int imageWidth, int imageHeight)
{
    _buffer = buffer;
    _capacity = capacity;
    _imageWidth = imageWidth;
    _imageHeight = imageHeight;
    reset();
}

void NfcSession::reset()
{
    _transferId = 0;
    _totalBytes = 0;
    _expectedCrc = 0;
    _receivedBytes = 0;
    _complete = false;
}

size_t NfcSession::handleHello(uint8_t *response, size_t capacity)
{
    // 送る側が分割の大きさと上限を決められるように、こちらの都合を伝える
    uint8_t payload[10];
    writeU16(payload, static_cast<uint16_t>(kMaxChunkSize));

    uint32_t maxImage = (_capacity < kMaxImageSize) ? static_cast<uint32_t>(_capacity) : kMaxImageSize;
    writeU32(payload + 2, maxImage);

    writeU16(payload + 6, static_cast<uint16_t>(_imageWidth));
    writeU16(payload + 8, static_cast<uint16_t>(_imageHeight));

    return buildResponse(CommandHello, StatusOk, payload, sizeof(payload), response, capacity);
}

size_t NfcSession::handleBegin(const Request &request, uint8_t *response, size_t capacity)
{
    if (request.totalBytes == 0 || request.totalBytes > _capacity || request.totalBytes > kMaxImageSize)
    {
        return buildResponse(CommandBegin, StatusTooLarge, nullptr, 0, response, capacity);
    }

    // 同じ内容でのやり直しは、受け取り済みの位置から続ける。
    // NFC はスマホを離すと切れるので、最初からやり直さずに済むようにする。
    bool resuming = (_transferId != 0) && (_totalBytes == request.totalBytes) && (_expectedCrc == request.crc32);
    if (!resuming)
    {
        _totalBytes = request.totalBytes;
        _expectedCrc = request.crc32;
        _receivedBytes = 0;
        _complete = false;

        // 転送ごとに変わる番号。内容から作るので、送る側と食い違わない。
        _transferId = crc32(reinterpret_cast<const uint8_t *>(&request.totalBytes), sizeof(request.totalBytes), request.crc32);
        if (_transferId == 0)
        {
            _transferId = 1;
        }
    }

    uint8_t payload[8];
    writeU32(payload, _transferId);
    writeU32(payload + 4, _receivedBytes);
    return buildResponse(CommandBegin, StatusAccepted, payload, sizeof(payload), response, capacity);
}

size_t NfcSession::handleData(const Request &request, uint8_t *response, size_t capacity)
{
    if (_transferId == 0 || request.transferId != _transferId)
    {
        return buildResponse(CommandData, StatusNoTransfer, nullptr, 0, response, capacity);
    }

    uint8_t payload[4];

    // 同じ場所が二度届いても受け入れる。応答が届かずに送り直されることがあるため。
    if (request.offset != _receivedBytes)
    {
        writeU32(payload, _receivedBytes);
        return buildResponse(CommandData, StatusBadOffset, payload, sizeof(payload), response, capacity);
    }

    if (request.payloadSize == 0 || request.offset + request.payloadSize > _totalBytes)
    {
        writeU32(payload, _receivedBytes);
        return buildResponse(CommandData, StatusTooLarge, payload, sizeof(payload), response, capacity);
    }

    for (size_t i = 0; i < request.payloadSize; i++)
    {
        _buffer[request.offset + i] = request.payload[i];
    }
    _receivedBytes = request.offset + static_cast<uint32_t>(request.payloadSize);

    writeU32(payload, _receivedBytes);
    return buildResponse(CommandData, StatusAccepted, payload, sizeof(payload), response, capacity);
}

size_t NfcSession::handleCommit(const Request &request, uint8_t *response, size_t capacity)
{
    if (_transferId == 0 || request.transferId != _transferId)
    {
        return buildResponse(CommandCommit, StatusNoTransfer, nullptr, 0, response, capacity);
    }
    if (_receivedBytes != _totalBytes)
    {
        return buildResponse(CommandCommit, StatusIncomplete, nullptr, 0, response, capacity);
    }
    if (crc32(_buffer, _totalBytes) != _expectedCrc)
    {
        // 壊れているので受け取り直す
        _receivedBytes = 0;
        return buildResponse(CommandCommit, StatusCrcMismatch, nullptr, 0, response, capacity);
    }

    _complete = true;
    return buildResponse(CommandCommit, StatusOk, nullptr, 0, response, capacity);
}

size_t NfcSession::handleAbort(const Request &request, uint8_t *response, size_t capacity)
{
    if (_transferId != 0 && request.transferId == _transferId)
    {
        reset();
    }
    return buildResponse(CommandAbort, StatusOk, nullptr, 0, response, capacity);
}

size_t NfcSession::handle(const uint8_t *request, size_t requestSize, uint8_t *response, size_t responseCapacity)
{
    if (_buffer == nullptr)
    {
        return 0;
    }

    Request parsed;
    if (!parseRequest(request, requestSize, parsed))
    {
        // 印や版が違うフレームには応答しない。他の機器へのやり取りかもしれない。
        return 0;
    }

    switch (parsed.command)
    {
    case CommandHello:
        return handleHello(response, responseCapacity);
    case CommandBegin:
        return handleBegin(parsed, response, responseCapacity);
    case CommandData:
        return handleData(parsed, response, responseCapacity);
    case CommandCommit:
        return handleCommit(parsed, response, responseCapacity);
    case CommandAbort:
        return handleAbort(parsed, response, responseCapacity);
    default:
        return buildResponse(parsed.command, StatusBadRequest, nullptr, 0, response, responseCapacity);
    }
}
