#include "Protocol.h"

namespace NfcProtocol
{
    uint32_t readU32(const uint8_t *data)
    {
        return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
               (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
    }

    void writeU32(uint8_t *data, uint32_t value)
    {
        data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
        data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[3] = static_cast<uint8_t>(value & 0xFF);
    }

    bool parseRequest(const uint8_t *data, size_t size, Request &out)
    {
        if (data == nullptr || size < kHeaderSize)
        {
            return false;
        }
        if (data[0] != kMagic0 || data[1] != kMagic1 || data[2] != kVersion)
        {
            return false;
        }

        out = Request();
        out.command = data[3];

        const uint8_t *body = data + kHeaderSize;
        size_t bodySize = size - kHeaderSize;

        switch (out.command)
        {
        case CommandHello:
            return true;

        case CommandBegin:
            // 大きさ(4) + CRC(4)
            if (bodySize < 8)
            {
                return false;
            }
            out.totalBytes = readU32(body);
            out.crc32 = readU32(body + 4);
            return true;

        case CommandData:
            // 転送の番号(4) + 位置(4) + 本体
            if (bodySize < 8)
            {
                return false;
            }
            out.transferId = readU32(body);
            out.offset = readU32(body + 4);
            out.payload = body + 8;
            out.payloadSize = bodySize - 8;
            return true;

        case CommandCommit:
        case CommandAbort:
            if (bodySize < 4)
            {
                return false;
            }
            out.transferId = readU32(body);
            return true;

        default:
            // 知らないコマンドでも、応答を返せるように読み解けたことにする
            return true;
        }
    }

    size_t buildResponse(uint8_t command, uint8_t status,
                         const uint8_t *payload, size_t payloadSize,
                         uint8_t *out, size_t outCapacity)
    {
        if (out == nullptr)
        {
            return 0;
        }

        size_t total = kResponseHeaderSize + payloadSize;
        if (total > outCapacity || total > kMaxFrameSize)
        {
            return 0;
        }

        out[0] = kMagic0;
        out[1] = kMagic1;
        out[2] = kVersion;
        out[3] = command;
        out[4] = status;

        for (size_t i = 0; i < payloadSize; i++)
        {
            out[kResponseHeaderSize + i] = payload[i];
        }
        return total;
    }

    uint32_t crc32(const uint8_t *data, size_t size, uint32_t seed)
    {
        // 表を持たずに計算する。実行のたびに作る必要がなく、
        // 1KB の表をメモリに置くほどの速さも要らない。
        uint32_t crc = seed ^ 0xFFFFFFFFu;
        for (size_t i = 0; i < size; i++)
        {
            crc ^= data[i];
            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc >> 1) ^ (0xEDB88320u & (~((crc & 1u) - 1u)));
            }
        }
        return crc ^ 0xFFFFFFFFu;
    }
}
