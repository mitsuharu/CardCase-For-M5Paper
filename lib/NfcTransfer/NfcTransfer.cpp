#include "NfcTransfer.h"

#if defined(ARDUINO) && defined(CARDCASE_HAS_NFC)

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <utility/M5IOE1_Class.hpp>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <esp_heap_caps.h>
#include <esp_mac.h>

#include "Session/Session.h"

#include <Storage.h>
#include <ImageSize.h>

namespace
{
    // 受け皿の大きさ。画像 1 枚ぶん。
    const size_t kBufferSize = NfcProtocol::kMaxImageSize;

    // NFC の電源は IO エキスパンダの gpio4 に繋がっている
    const auto kNfcEnablePin = m5::M5IOE1_Class::gpio4;

    /**
     * タグとして見せる姿。
     *
     * ライブラリがエミュレーションできるのは NTAG2 系か MIFARE Ultralight だけ。
     * こちらは独自のコマンドでやり取りするのでタグの中身は使わないが、
     * スマホに「タグがある」と認識させるために名乗りが要る。
     * 一番小さい NTAG213 を選んでメモリを節約する。
     */
    const auto kPiccType = m5::nfc::a::Type::NTAG_213;

    // NTAG の UID は 7 バイト
    const uint8_t kUidLength = 7;

    // NTAG213 の全体（45 ページ × 4 バイト）に収まる大きさ
    uint8_t piccMemory[256];

    m5::unit::UnitUnified units;
    m5::unit::UnitST25R3916 unit;

    NfcSession session;
    uint8_t *buffer = nullptr;
    uint8_t response[NfcProtocol::kMaxFrameSize];

    bool ready = false;
    bool listening = false;
    String receivedPath;

    /**
     * 受け取った画像を SD に残す。SD が無ければ何もしない。
     *
     * 一覧では [NFC] から来たものと分かるようにこの名前にする。
     * 送るたびに上書きするので SD が埋まらない。形式は JPEG と PNG の
     * どちらもありうるので、拡張子違いが残らないようまとめて消してから書く。
     */
    const char *kReceivedPrefix = "/NFC";
    const char *kReceivedExtensions[] = {".png", ".jpg", ".jpeg"};

    String saveToStorage(const uint8_t *data, size_t size)
    {
        if (!Storage::isAvailable() || data == nullptr || size == 0)
        {
            return String();
        }

        const char *extension = ImageFile::extensionFor(data, size);
        if (extension == nullptr)
        {
            M5.Log(esp_log_level_t::ESP_LOG_WARN, "nfc: unknown image format\n");
            return String();
        }

        for (size_t i = 0; i < sizeof(kReceivedExtensions) / sizeof(kReceivedExtensions[0]); i++)
        {
            String path = String(kReceivedPrefix) + kReceivedExtensions[i];
            if (Storage::fs().exists(path))
            {
                Storage::fs().remove(path);
            }
        }

        String path = String(kReceivedPrefix) + extension;
        File file = Storage::fs().open(path, FILE_WRITE);
        if (!file)
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: cannot open %s\n", path.c_str());
            return String();
        }

        size_t written = file.write(data, size);
        file.close();

        if (written != size)
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: write failed\n");
            return String();
        }

        M5.Log(esp_log_level_t::ESP_LOG_INFO, "nfc: saved %s (%u bytes)\n", path.c_str(), (unsigned)size);
        return path;
    }

    /**
     * 受け取ったフレームに応答する。
     *
     * ライブラリの既定の実装は普通のタグとして振る舞うが、
     * こちらは独自のコマンドをやり取りするので差し替える。
     */
    class Emulation : public m5::nfc::EmulationLayerA
    {
    public:
        explicit Emulation(m5::unit::UnitST25R3916 &u) : EmulationLayerA(u), _unit(u) {}

        State receive_callback(const uint8_t *rx, const uint32_t rx_len) override
        {
            size_t length = session.handle(rx, rx_len, response, sizeof(response));
            if (length == 0)
            {
                // こちらへのやり取りではないので、既定の振る舞いに任せる
                return EmulationLayerA::receive_callback(rx, rx_len);
            }

            // フレームごとにログを出すと往復が増えて転送が遅くなるので出さない
            return _unit.nfcaEmulationTransmit(response, static_cast<uint16_t>(length))
                       ? State::Active
                       : State::Idle;
        }

    private:
        m5::unit::UnitST25R3916 &_unit;
    };

    Emulation *emulation = nullptr;

    /**
     * NFC の電源を入れ直す。
     *
     * IO エキスパンダは本体を再起動しても状態が残るので、単に HIGH を書くだけでは
     * 前回の中途半端な設定のままになることがある。一度 LOW に落としてから上げる。
     */
    void resetNfcPower()
    {
        auto &ioe = M5.getIOExpander(0);
        ioe.setHighImpedance(kNfcEnablePin, false);
        ioe.setDirection(kNfcEnablePin, true);
        ioe.digitalWrite(kNfcEnablePin, false);
        delay(20);
        ioe.digitalWrite(kNfcEnablePin, true);
        delay(100);
    }
}

namespace NfcTransfer
{
    bool begin(const DeviceProfile &profile)
    {
        if (!profile.hasNfc)
        {
            return false;
        }

        if (buffer == nullptr)
        {
            buffer = static_cast<uint8_t *>(heap_caps_malloc(kBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (buffer == nullptr)
            {
                M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: out of memory\n");
                return false;
            }
        }
        session.begin(buffer, kBufferSize, profile.width, profile.height);

        resetNfcPower();

        auto config = unit.config();
        config.emulation = true;
        config.mode = m5::nfc::NFC::A;
        // 割り込みを使わず register を読みに行く。
        // 受け身の状態へ移るときに割り込みの端子に頼ると取りこぼすため。
        config.using_irq = false;
        unit.config(config);

        if (!ready)
        {
            if (!m5::unit::wiring::i2cClass(units, unit, M5.In_I2C))
            {
                M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: cannot add unit\n");
                return false;
            }
            ready = units.begin();
        }
        else
        {
            ready = unit.begin();
        }

        if (!ready)
        {
            uint8_t type = 0;
            uint8_t revision = 0;
            bool identity = unit.readICIdentity(type, revision);
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: begin failed identity=%d type=%02X rev=%02X\n",
                   identity ? 1 : 0, type, revision);
            return false;
        }

        if (emulation == nullptr)
        {
            emulation = new Emulation(unit);
        }

        // UID は MAC から作る。同じ本体なら毎回同じになるので、
        // スマホ側が別のタグとして扱わずに済む。
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        uint8_t uid[kUidLength] = {0x04}; // 先頭は NXP の製造者コード
        for (int i = 0; i < 6; i++)
        {
            uid[i + 1] = mac[i];
        }

        m5::nfc::a::PICC picc{};
        if (!picc.emulate(kPiccType, uid, kUidLength))
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: cannot configure picc\n");
            return false;
        }
        if (picc.totalSize() > sizeof(piccMemory))
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: picc memory too small %u\n", picc.totalSize());
            return false;
        }

        listening = emulation->begin(picc, piccMemory, sizeof(piccMemory));
        if (!listening)
        {
            M5.Log(esp_log_level_t::ESP_LOG_ERROR, "nfc: emulation begin failed\n");
            return false;
        }

        M5.Log(esp_log_level_t::ESP_LOG_INFO, "nfc: listening\n");
        return true;
    }

    void update()
    {
        if (!listening || emulation == nullptr)
        {
            return;
        }
        units.update();
        emulation->update();
    }

    void end()
    {
        if (emulation != nullptr && listening)
        {
            emulation->end();
        }
        listening = false;

        // 電波を止めて電池を使わないようにする
        auto &ioe = M5.getIOExpander(0);
        ioe.digitalWrite(kNfcEnablePin, false);
    }

    bool hasReceivedImage()
    {
        if (!session.isComplete())
        {
            return false;
        }
        // 受け取り切った時点で一度だけ保存する
        if (receivedPath.length() == 0)
        {
            receivedPath = saveToStorage(session.image(), session.imageSize());
        }
        return true;
    }

    String receivedImagePath()
    {
        return receivedPath;
    }

    const uint8_t *receivedImage(size_t &size)
    {
        size = session.imageSize();
        return session.image();
    }

    void releaseReceivedImage()
    {
        session.reset();
        receivedPath = "";
    }

    uint32_t receivedBytes()
    {
        return session.receivedBytes();
    }

    uint32_t totalBytes()
    {
        return static_cast<uint32_t>(session.imageSize());
    }
}

#endif
