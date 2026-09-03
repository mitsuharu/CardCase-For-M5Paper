#include <SD.h>
#include <M5Unified.h>
#include <M5Helper.h>
#include <DeviceProfile.h>
#include <ImageFile.h>
#include <Menu.h>

// M5Unified が機種を自動判定できなかったときの保険。
// platformio.ini の env ごとに -DCARDCASE_FALLBACK_BOARD で指定する。
#ifndef CARDCASE_FALLBACK_BOARD
#define CARDCASE_FALLBACK_BOARD board_unknown
#endif

// M5PaperMono のフロントライトの明るさ（電池のため控えめにする）
#define FRONTLIGHT_BRIGHTNESS 64

// SD のマウントを何回まで試すか
#define SD_MOUNT_RETRY_COUNT 3
#define SD_MOUNT_RETRY_INTERVAL_MS 200

DeviceProfile profile;
Menu menu;

/**
 * SD をアンマウントしてからディープスリープに入る。
 *
 * ディープスリープ中も SD カードには通電したままなので、読み出しの途中で寝ると
 * カードが中途半端な状態で残り、復帰後の SD.begin() が失敗する。
 * M5PaperS3 は M5GFX 側が SD を SPI モードに入れ直してくれない機種なので、
 * アプリ側で後始末する必要がある。
 */
void enterDeepSleep()
{
  SD.end();
  SPI.end();

  // 電池のためスリープに入る。再び画像選択したい場合は電源ボタンを押す。
  M5.Log(esp_log_level_t::ESP_LOG_INFO, "Deep sleep start\n");
  M5.Power.deepSleep();
}

/// 一覧で選ばれたときに全画面表示してスリープに入る
void onSelectImage(const MenuItem &item)
{
  M5Helper::drawImageFromSD(item.value, profile);
  enterDeepSleep();
}

/// SD をマウントする。復帰直後は失敗することがあるのでやり直す。
bool mountSD()
{
  auto mosi = M5.getPin(m5::pin_name_t::sd_spi_mosi);
  auto miso = M5.getPin(m5::pin_name_t::sd_spi_miso);
  auto sclk = M5.getPin(m5::pin_name_t::sd_spi_sclk);
  auto cs = M5.getPin(m5::pin_name_t::sd_spi_cs);

  for (int i = 0; i < SD_MOUNT_RETRY_COUNT; i++)
  {
    if (i > 0)
    {
      // 一度落としてから初期化し直す
      SD.end();
      SPI.end();
      delay(SD_MOUNT_RETRY_INTERVAL_MS);
      M5.Log(esp_log_level_t::ESP_LOG_INFO, "retry to mount SD card (%d)\n", i);
    }

    SPI.begin(sclk, miso, mosi);
    if (SD.begin(cs, SPI, 4000000))
    {
      return true;
    }
  }
  return false;
}

/// 致命的なエラーを表示して停止する
void halt(const String &message)
{
  M5.Display.println(message);
  M5.Display.waitDisplay();
  while (1)
  {
    delay(1000);
  }
}

void setup()
{
  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::CARDCASE_FALLBACK_BOARD;
  M5.begin(cfg);

  profile = currentProfile();

  // スリープから起きる。実機の電源ボタンを押してください。
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  M5.Log(esp_log_level_t::ESP_LOG_INFO, "Wakeup reason: %d\n", wakeup_reason);
  M5.Log(esp_log_level_t::ESP_LOG_INFO, "Board: %s\n", profile.name);

  if (profile.hasFrontlight)
  {
    M5.Display.setBrightness(FRONTLIGHT_BRIGHTNESS);
  }

  M5.Display.setRotation(static_cast<uint_fast8_t>(DeviceRotation::Up));
  if (M5.Display.isEPD())
  {
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  }
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

  // 画面幅に合わせた見出しの文字サイズ
  int headerTextSize = profile.menuTextSize / 2;
  if (headerTextSize < 2)
  {
    headerTextSize = 2;
  }
  M5.Display.setTextSize(headerTextSize);
  M5.Display.printf("CardCase for %s\n", profile.name);

  if (profile.kind == DeviceKind::Unknown)
  {
    halt("unsupported board.");
  }

  // SD の初期化
  if (!mountSD())
  {
    halt("mount SD card .. NG");
  }
  M5.Display.println("mount SD card .. OK");

  // SD のルートディレクトリを開く
  File root = SD.open("/");
  if (!root)
  {
    halt("failed to open SD card");
  }

  // SD を走査して画像ファイルをリストアップ
  File file = root.openNextFile();
  while (file)
  {
    if (!file.isDirectory())
    {
      String filename = file.name();
      if (ImageFile::isListable(filename) && !menu.addItem(filename, ImageFile::rootPath(filename)))
      {
        // 一覧の上限に達した
        file.close();
        break;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  if (menu.itemCount() == 0)
  {
    halt("It does not found images.");
  }

  if (!profile.isOperable())
  {
    // タッチもボタンも無い＝機種判定かビルド設定が誤っている
    halt("no input available.");
  }

  // 見出しの下から一覧を並べる
  M5.Display.println("");
  menu.begin(profile, M5.Display.getCursorY(), onSelectImage);
}

void loop()
{
  M5.update();
  menu.update();
  delay(10);
}
