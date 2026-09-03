#include <M5Unified.h>
#include <Storage.h>
#include <M5Helper.h>
#include <DeviceProfile.h>
#include <ImageFile.h>
#include <Menu.h>
#include <WebTransfer.h>

// M5Unified が機種を自動判定できなかったときの保険。
// platformio.ini の env ごとに -DCARDCASE_FALLBACK_BOARD で指定する。
#ifndef CARDCASE_FALLBACK_BOARD
#define CARDCASE_FALLBACK_BOARD board_unknown
#endif

// M5PaperMono のフロントライトの明るさ（電池のため控えめにする）
#define FRONTLIGHT_BRIGHTNESS 64

// 画像を表示したあと、ボタンやタッチで一覧に戻れる時間。
// これを過ぎたら電池のためスリープに入る。
#define VIEWING_TIMEOUT_MS 60000

/// いま画面に出しているもの
enum class Mode
{
  Browsing,     // 一覧
  Viewing,      // 画像
  Transferring, // WiFi で画像を待っている
};

DeviceProfile profile;
Menu menu;

void drawHeader();
void halt(const String &message);
Mode mode = Mode::Browsing;
unsigned long viewingUntil = 0;

/// SD を後始末してからディープスリープに入る
void enterDeepSleep()
{
  Storage::end();

  // 電池のためスリープに入る。再び画像選択したい場合は電源ボタンを押す。
  M5.Log(esp_log_level_t::ESP_LOG_INFO, "Deep sleep start\n");
  M5.Power.deepSleep();
}

/// 画像を全画面表示して、戻れる状態にする
void showImage(const String &path)
{
  M5Helper::drawImageFromSD(path, profile);

  // 電源ボタンで復帰すると M5.begin() がパネルを初期化し直すため、
  // 電子ペーパーが大きく点滅する。ボタンのある機種では、すぐ寝ずに
  // しばらく起きたままにして、ボタンで一覧に戻れるようにするとそれを避けられる。
  if (!profile.hasButtons())
  {
    // 戻る手段が無いなら起きている意味がない
    enterDeepSleep();
    return;
  }

  mode = Mode::Viewing;
  viewingUntil = millis() + VIEWING_TIMEOUT_MS;
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

/// 画像を表示している状態から一覧へ戻る
void returnToMenu()
{
  M5.Display.startWrite();

  // 画像は EXIF や画面合わせで回してあるので、一覧の向きに戻す
  M5.Display.setRotation(static_cast<uint_fast8_t>(DeviceRotation::Up));
  if (M5.Display.isEPD())
  {
    // 一覧に戻るだけなので、画質より速さと点滅の少なさを優先する
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  }

  M5.Display.fillScreen(TFT_WHITE);
  drawHeader();
  menu.render();

  M5.Display.endWrite();

  mode = Mode::Browsing;
}

/**
 * 画像を見ている間に、一覧へ戻る操作があったか。
 *
 * 物理ボタンだけを見る。相手に画面を見せている最中の誤タップで
 * 画像が消えてしまわないよう、タッチでは戻さない。
 * ボタンを持たない機種（M5PaperS3）では戻れないので、
 * 従来どおりスリープしてから電源ボタンで復帰する。
 */
bool wasReturnPressed()
{
  return M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed();
}

/// WiFi で画像を受け取る状態に入る
void startTransfer()
{
  if (!WebTransfer::begin(profile))
  {
    halt("failed to start WiFi.");
  }

  M5.Display.startWrite();
  M5.Display.setRotation(static_cast<uint_fast8_t>(DeviceRotation::Up));
  M5.Display.fillScreen(TFT_WHITE);
  drawHeader();
  M5.Display.endWrite();

  WebTransfer::render(profile, M5.Display.getCursorY());
  mode = Mode::Transferring;
}

/// 一覧で選ばれたときの処理
void onSelectItem(const MenuItem &item)
{
  if (item.kind == MenuItemKind::Transfer)
  {
    startTransfer();
    return;
  }
  showImage(item.value);
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
      if (ImageFile::isListable(filename) && !menu.addItem(MenuItemKind::Image, filename, ImageFile::rootPath(filename)))
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
  // SD が無くても WiFi で画像を受け取って表示はできるので、ここでは止めない
  if (Storage::begin())
  {
    collectImages();
  }
  else
  {
    M5.Log(esp_log_level_t::ESP_LOG_WARN, "SD card is not available\n");
  }

  // WiFi で受け取る導線を先頭に置く。
  // 画像が 1 枚も無くてもここから追加できるので、halt させない。
  menu.addItem(MenuItemKind::Transfer, "[Receive by WiFi]", "");

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
  menu.begin(profile, M5.Display.getCursorY(), onSelectItem);
  M5.Display.endWrite();
}

void loop()
{
  M5.update();

  if (mode == Mode::Transferring)
  {
    WebTransfer::update();

    if (WebTransfer::hasReceivedImage())
    {
      // 受け取ったらすぐ表示する。待つ必要はないので電波は止める。
      WebTransfer::end();

      size_t size = 0;
      const uint8_t *image = WebTransfer::receivedImage(size);
      M5Helper::drawImageFromMemory(image, size, profile);
      WebTransfer::releaseReceivedImage();

      mode = Mode::Viewing;
      viewingUntil = millis() + VIEWING_TIMEOUT_MS;
    }
    else if (wasReturnPressed())
    {
      WebTransfer::end();
      returnToMenu();
    }
    delay(5);
    return;
  }

  if (mode == Mode::Viewing)
  {
    if (wasReturnPressed())
    {
      returnToMenu();
    }
    else if (static_cast<long>(millis() - viewingUntil) >= 0)
    {
      // 電池のためスリープに入る。再び画像を選びたい場合は電源ボタンを押す。
      enterDeepSleep();
    }
    delay(10);
    return;
  }

  menu.update();
  delay(10);
}
