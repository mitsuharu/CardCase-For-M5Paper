#include <unity.h>
#include <Protocol/Protocol.h>
#include <Session/Session.h>
#include <vector>
#include <string.h>

using namespace NfcProtocol;

void setUp(void) {}
void tearDown(void) {}

namespace
{
    const int kWidth = 480;
    const int kHeight = 800;

    // 受け皿。実機では PSRAM から取る。
    uint8_t buffer[4096];
    uint8_t response[kMaxFrameSize];

    std::vector<uint8_t> frame(uint8_t command, const std::vector<uint8_t> &body)
    {
        std::vector<uint8_t> f = {kMagic0, kMagic1, kVersion, command};
        f.insert(f.end(), body.begin(), body.end());
        return f;
    }

    std::vector<uint8_t> u32(uint32_t value)
    {
        std::vector<uint8_t> v(4);
        writeU32(v.data(), value);
        return v;
    }

    std::vector<uint8_t> concat(std::vector<uint8_t> a, const std::vector<uint8_t> &b)
    {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }

    /// 送る側のふるまいを真似て、画像を分割して送り切る
    struct Sender
    {
        NfcSession &session;
        std::vector<uint8_t> image;
        uint32_t transferId = 0;

        size_t send(const std::vector<uint8_t> &request)
        {
            return session.handle(request.data(), request.size(), response, sizeof(response));
        }

        uint8_t status() const { return response[4]; }

        void begin()
        {
            uint32_t sum = crc32(image.data(), image.size());
            send(frame(CommandBegin, concat(u32((uint32_t)image.size()), u32(sum))));
            transferId = readU32(response + 5);
        }

        /// 続きの位置から chunk ずつ送る
        void sendAll(size_t chunk)
        {
            uint32_t offset = readU32(response + 9);
            while (offset < image.size())
            {
                size_t length = image.size() - offset;
                if (length > chunk) { length = chunk; }

                std::vector<uint8_t> body = concat(u32(transferId), u32(offset));
                body.insert(body.end(), image.begin() + offset, image.begin() + offset + length);
                send(frame(CommandData, body));
                offset = readU32(response + 5);
            }
        }

        void commit() { send(frame(CommandCommit, u32(transferId))); }
    };

    std::vector<uint8_t> makeImage(size_t size)
    {
        std::vector<uint8_t> image(size);
        for (size_t i = 0; i < size; i++)
        {
            image[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);
        }
        return image;
    }
}

void test_frame_limits_fit_iso14443a()
{
    // ISO14443-A は CRC を除いて 253 バイトまで。超えると RF の層で弾かれる。
    TEST_ASSERT_EQUAL_INT(253, (int)kMaxFrameSize);
    TEST_ASSERT_TRUE(kHeaderSize + 4 + 4 + kMaxChunkSize <= kMaxFrameSize);
    // 見出し(4) + 転送の番号(4) + 位置(4) を引いた残り
    TEST_ASSERT_EQUAL_INT(241, (int)kMaxChunkSize);
}

void test_rejects_foreign_frames()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    // 印が違うフレームには応答しない。他の機器とのやり取りかもしれない。
    const uint8_t foreign[] = {'X', 'X', 1, CommandHello};
    TEST_ASSERT_EQUAL_INT(0, (int)session.handle(foreign, sizeof(foreign), response, sizeof(response)));

    // 版が違う場合も同じ
    const uint8_t otherVersion[] = {kMagic0, kMagic1, 99, CommandHello};
    TEST_ASSERT_EQUAL_INT(0, (int)session.handle(otherVersion, sizeof(otherVersion), response, sizeof(response)));

    // 短すぎるフレーム
    const uint8_t truncated[] = {kMagic0, kMagic1};
    TEST_ASSERT_EQUAL_INT(0, (int)session.handle(truncated, sizeof(truncated), response, sizeof(response)));
}

void test_hello_reports_capabilities()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    auto request = frame(CommandHello, {});
    size_t length = session.handle(request.data(), request.size(), response, sizeof(response));

    TEST_ASSERT_EQUAL_INT((int)(kResponseHeaderSize + 10), (int)length);
    TEST_ASSERT_EQUAL_UINT8(StatusOk, response[4]);

    // 分割の大きさ、受け取れる上限、画面の大きさを伝える
    TEST_ASSERT_EQUAL_INT((int)kMaxChunkSize, (response[5] << 8) | response[6]);
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer), readU32(response + 7));
    TEST_ASSERT_EQUAL_INT(kWidth, (response[11] << 8) | response[12]);
    TEST_ASSERT_EQUAL_INT(kHeight, (response[13] << 8) | response[14]);
}

void test_transfers_an_image()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, makeImage(1000)};
    sender.begin();
    TEST_ASSERT_EQUAL_UINT8(StatusAccepted, sender.status());

    sender.sendAll(kMaxChunkSize);
    sender.commit();

    TEST_ASSERT_EQUAL_UINT8(StatusOk, sender.status());
    TEST_ASSERT_TRUE(session.isComplete());
    TEST_ASSERT_EQUAL_UINT32(1000, session.imageSize());
    TEST_ASSERT_EQUAL_INT(0, memcmp(session.image(), sender.image.data(), 1000));
}

void test_transfer_works_with_small_chunks()
{
    // 通信が不安定なときは送る側が分割を小さくすることがある
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, makeImage(500)};
    sender.begin();
    sender.sendAll(64);
    sender.commit();

    TEST_ASSERT_TRUE(session.isComplete());
    TEST_ASSERT_EQUAL_INT(0, memcmp(session.image(), sender.image.data(), 500));
}

void test_resumes_after_interruption()
{
    // NFC はスマホを離すと切れる。同じ内容で BEGIN をやり直せば続きから送れる。
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, makeImage(1000)};
    sender.begin();
    uint32_t firstId = sender.transferId;

    // 途中まで送って中断する
    sender.send(frame(CommandData, concat(concat(u32(sender.transferId), u32(0)),
                                          std::vector<uint8_t>(sender.image.begin(), sender.image.begin() + 200))));
    TEST_ASSERT_EQUAL_UINT32(200, session.receivedBytes());

    // やり直すと、同じ番号と続きの位置が返る
    sender.begin();
    TEST_ASSERT_EQUAL_UINT32(firstId, sender.transferId);
    TEST_ASSERT_EQUAL_UINT32(200, readU32(response + 9));

    sender.sendAll(kMaxChunkSize);
    sender.commit();

    TEST_ASSERT_TRUE(session.isComplete());
    TEST_ASSERT_EQUAL_INT(0, memcmp(session.image(), sender.image.data(), 1000));
}

void test_rejects_wrong_offset()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, makeImage(500)};
    sender.begin();

    // 飛ばした位置は受け付けず、続きの位置を教える
    sender.send(frame(CommandData, concat(concat(u32(sender.transferId), u32(100)), {1, 2, 3})));
    TEST_ASSERT_EQUAL_UINT8(StatusBadOffset, sender.status());
    TEST_ASSERT_EQUAL_UINT32(0, readU32(response + 5));
    TEST_ASSERT_EQUAL_UINT32(0, session.receivedBytes());
}

void test_detects_broken_image()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    auto image = makeImage(300);
    uint32_t sum = crc32(image.data(), image.size());

    // 途中で 1 バイト書き換えて送る
    auto broken = image;
    broken[100] ^= 0xFF;

    Sender sender{session, broken};
    sender.send(frame(CommandBegin, concat(u32((uint32_t)image.size()), u32(sum))));
    sender.transferId = readU32(response + 5);
    sender.sendAll(kMaxChunkSize);
    sender.commit();

    TEST_ASSERT_EQUAL_UINT8(StatusCrcMismatch, sender.status());
    TEST_ASSERT_FALSE(session.isComplete());

    // 受け取り直せるよう位置は戻る
    TEST_ASSERT_EQUAL_UINT32(0, session.receivedBytes());
}

void test_rejects_commit_before_all_data()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, makeImage(500)};
    sender.begin();
    sender.send(frame(CommandData, concat(concat(u32(sender.transferId), u32(0)), {1, 2, 3})));
    sender.commit();

    TEST_ASSERT_EQUAL_UINT8(StatusIncomplete, sender.status());
    TEST_ASSERT_FALSE(session.isComplete());
}

void test_rejects_unknown_transfer()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, makeImage(100)};
    sender.begin();

    // 番号が違えば受け付けない
    sender.send(frame(CommandData, concat(concat(u32(sender.transferId + 1), u32(0)), {1, 2, 3})));
    TEST_ASSERT_EQUAL_UINT8(StatusNoTransfer, sender.status());
}

void test_rejects_oversized_image()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, {}};
    sender.send(frame(CommandBegin, concat(u32(sizeof(buffer) + 1), u32(0))));
    TEST_ASSERT_EQUAL_UINT8(StatusTooLarge, sender.status());

    // 大きさ 0 も受け付けない
    sender.send(frame(CommandBegin, concat(u32(0), u32(0))));
    TEST_ASSERT_EQUAL_UINT8(StatusTooLarge, sender.status());
}

void test_abort_clears_the_transfer()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    Sender sender{session, makeImage(500)};
    sender.begin();
    sender.send(frame(CommandData, concat(concat(u32(sender.transferId), u32(0)), {1, 2, 3})));
    TEST_ASSERT_EQUAL_UINT32(3, session.receivedBytes());

    sender.send(frame(CommandAbort, u32(sender.transferId)));
    TEST_ASSERT_EQUAL_UINT8(StatusOk, sender.status());
    TEST_ASSERT_EQUAL_UINT32(0, session.receivedBytes());
}

void test_unknown_command_is_answered()
{
    NfcSession session;
    session.begin(buffer, sizeof(buffer), kWidth, kHeight);

    // 知らないコマンドでも黙らず、形式が違うと返す
    auto request = frame(0x7F, {});
    size_t length = session.handle(request.data(), request.size(), response, sizeof(response));
    TEST_ASSERT_EQUAL_INT((int)kResponseHeaderSize, (int)length);
    TEST_ASSERT_EQUAL_UINT8(StatusBadRequest, response[4]);
}

void test_crc32_matches_known_values()
{
    // IEEE 802.3 の CRC-32。送る側と食い違わないよう既知の値で確かめる。
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, crc32((const uint8_t *)"123456789", 9));
    TEST_ASSERT_EQUAL_UINT32(0x00000000u, crc32(nullptr, 0));
    TEST_ASSERT_EQUAL_UINT32(0xD202EF8Du, crc32((const uint8_t *)"\x00", 1));
}

void test_matches_the_documented_examples()
{
    // docs/nfc-protocol.md に載せているバイト列そのもの。
    // 送る側を自分で実装する人はこの文書を頼りにするので、
    // 実装を変えて文書と食い違ったらここで気付けるようにしている。
    NfcSession session;
    session.begin(buffer, sizeof(buffer), 480, 800);

    struct Case
    {
        const char *label;
        std::vector<uint8_t> request;
        std::vector<uint8_t> response;
    };

    const std::vector<Case> cases = {
        {"HELLO",
         {0x43, 0x43, 0x01, 0x01},
         {0x43, 0x43, 0x01, 0x01, 0x00, 0x00, 0xF1, 0x00, 0x00, 0x10, 0x00, 0x01, 0xE0, 0x03, 0x20}},
        {"BEGIN",
         {0x43, 0x43, 0x01, 0x02, 0x00, 0x00, 0x00, 0x06, 0x86, 0xD3, 0xF8, 0xC2},
         {0x43, 0x43, 0x01, 0x02, 0x01, 0xCD, 0x72, 0x34, 0xEC, 0x00, 0x00, 0x00, 0x00}},
        {"DATA",
         {0x43, 0x43, 0x01, 0x03, 0xCD, 0x72, 0x34, 0xEC, 0x00, 0x00, 0x00, 0x00,
          0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10},
         {0x43, 0x43, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x06}},
        {"COMMIT",
         {0x43, 0x43, 0x01, 0x04, 0xCD, 0x72, 0x34, 0xEC},
         {0x43, 0x43, 0x01, 0x04, 0x00}},
    };

    for (const auto &c : cases)
    {
        size_t length = session.handle(c.request.data(), c.request.size(), response, sizeof(response));
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)c.response.size(), (int)length, c.label);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(c.response.data(), response, c.response.size(), c.label);
    }

    // 文書に載せている CRC の期待値
    const uint8_t sample[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};
    TEST_ASSERT_EQUAL_UINT32(0x86D3F8C2u, crc32(sample, sizeof(sample)));
}

int runUnityTests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_frame_limits_fit_iso14443a);
    RUN_TEST(test_rejects_foreign_frames);
    RUN_TEST(test_hello_reports_capabilities);
    RUN_TEST(test_transfers_an_image);
    RUN_TEST(test_transfer_works_with_small_chunks);
    RUN_TEST(test_resumes_after_interruption);
    RUN_TEST(test_rejects_wrong_offset);
    RUN_TEST(test_detects_broken_image);
    RUN_TEST(test_rejects_commit_before_all_data);
    RUN_TEST(test_rejects_unknown_transfer);
    RUN_TEST(test_rejects_oversized_image);
    RUN_TEST(test_abort_clears_the_transfer);
    RUN_TEST(test_unknown_command_is_answered);
    RUN_TEST(test_crc32_matches_known_values);
    RUN_TEST(test_matches_the_documented_examples);
    return UNITY_END();
}

int main(void)
{
    return runUnityTests();
}
