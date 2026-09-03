#include <M5Unified.h>
#include <Storage.h>
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

DeviceProfile profile;
Menu menu;

/// SD を後始末してからディープスリープに入る
void enterDeepSleep()
{
  Storage::end();

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

/// 画面幅に合わせた見出しを描く
void drawHeader()
{
  int headerTextSize = profile.menuTextSize / 2;
  if (headerTextSize < 2)
  {
    headerTextSize = 2;
  }
  M5.Display.setTextSize(headerTextSize);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setCursor(0, 0);
  M5.Display.printf("CardCase for %s\n", profile.name);
}

/// 致命的なエラーを表示して停止する
void halt(const String &message)
{
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_WHITE);
  drawHeader();
  M5.Display.println(message);
  M5.Display.endWrite();
  M5.Display.waitDisplay();

  while (1)
  {
    delay(1000);
  }
}

/// SD を走査して画像ファイルを一覧に積む
bool collectImages()
{
  File root = Storage::fs().open("/");
  if (!root)
  {
    return false;
  }

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

  return true;
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

  if (profile.kind == DeviceKind::Unknown)
  {
    halt("unsupported board.");
  }

  // 電子ペーパーは endWrite のたびに画面を更新し、M5PaperColor では
  // 1 回あたり十数秒かかる。進捗を逐一表示すると起動が数分になるので、
  // SD の処理を先に終わらせてから、画面は最後に 1 回だけ描く。
  if (!Storage::begin())
  {
    halt("mount SD card .. NG");
  }

  if (!collectImages())
  {
    halt("failed to open SD card");
  }

  if (menu.itemCount() == 0)
  {
    halt("It does not found images.");
  }

  if (!profile.isOperable())
  {
    // タッチもボタンも無い＝機種判定かビルド設定が誤っている
    halt("no input available.");
  }

  // 見出しから一覧まで、まとめて 1 回で描く
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_WHITE);
  drawHeader();
  M5.Display.println("");
  menu.begin(profile, M5.Display.getCursorY(), onSelectImage);
  M5.Display.endWrite();
}

void loop()
{
  M5.update();
  menu.update();
  delay(10);
}
