#include "Storage.h"

#ifdef ARDUINO

#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <M5Unified.h>
#include <driver/gpio.h>
#include "SdPins/SdPins.h"

namespace
{
    // マウントを何回まで試すか
    const int kRetryCount = 3;
    const int kRetryIntervalMs = 200;

    const uint32_t kSpiFrequency = 4000000;

    bool usingSdmmc = false;
    bool mounted = false;

    /// M5Unified のピンテーブルから配線を読む
    SdPins readSdPins()
    {
        SdPins pins;
        pins.clk = M5.getPin(m5::pin_name_t::sd_mmc_clk);
        pins.cmd = M5.getPin(m5::pin_name_t::sd_mmc_cmd);
        pins.d0 = M5.getPin(m5::pin_name_t::sd_mmc_d0);
        pins.d1 = M5.getPin(m5::pin_name_t::sd_mmc_d1);
        pins.d2 = M5.getPin(m5::pin_name_t::sd_mmc_d2);
        pins.d3 = M5.getPin(m5::pin_name_t::sd_mmc_d3);
        return pins;
    }

    bool beginSpi(const SdPins &pins)
    {
        // SPI では cmd が MOSI、d0 が MISO、d3 が CS にあたる
        SPI.begin(pins.clk, pins.d0, pins.cmd);
        return SD.begin(pins.d3, SPI, kSpiFrequency);
    }

    bool beginSdmmc(const SdPins &pins)
    {
        int8_t pullUps[SdPins::kMaxPullUpPins];
        int count = pins.pullUpPins(pullUps, SdPins::kMaxPullUpPins);
        for (int i = 0; i < count; i++)
        {
            gpio_set_pull_mode(static_cast<gpio_num_t>(pullUps[i]), GPIO_PULLUP_ONLY);
        }

        if (!SD_MMC.setPins(pins.clk, pins.cmd, pins.d0, pins.d1, pins.d2, pins.d3))
        {
            return false;
        }
        return SD_MMC.begin("/sdcard", false, false);
    }
}

namespace Storage
{
    bool begin()
    {
        // つなぎ方は M5Unified のピンテーブルが知っている
        mounted = false;

        SdPins pins = readSdPins();
        if (!pins.isValid())
        {
            return false;
        }
        usingSdmmc = pins.usesSdmmc();

        for (int i = 0; i < kRetryCount; i++)
        {
            if (i > 0)
            {
                // 一度落としてから初期化し直す
                end();
                delay(kRetryIntervalMs);
                M5.Log(esp_log_level_t::ESP_LOG_INFO, "retry to mount SD card (%d)\n", i);
            }

            if (usingSdmmc ? beginSdmmc(pins) : beginSpi(pins))
            {
                mounted = true;
                return true;
            }
        }
        return false;
    }

    bool isAvailable()
    {
        return mounted;
    }

    void end()
    {
        mounted = false;

        if (usingSdmmc)
        {
            SD_MMC.end();
            return;
        }

        SD.end();
        SPI.end();
    }

    fs::FS &fs()
    {
        if (usingSdmmc)
        {
            return SD_MMC;
        }
        return SD;
    }
}

#endif
