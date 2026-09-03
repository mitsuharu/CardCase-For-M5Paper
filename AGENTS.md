# AGENTS.md

このリポジトリで作業するときの開発ルールをまとめる。人間もエージェントもここを参照する。

## プロジェクト概要

M5Stack の電子ペーパー製品向けのアプリ。SD カードに保存した画像を一覧から選び、全画面に表示して名刺のように使う。表示したあとはディープスリープに入り、電源ボタンで復帰して選び直す。

開発環境は VS Code + PlatformIO。

## 対応機種

| | M5Paper v1.1 | M5PaperS3 | M5PaperColor | M5PaperMono |
|---|---|---|---|---|
| SoC | ESP32 | ESP32-S3 | ESP32-S3 | ESP32-S3 |
| PlatformIO env | `M5Paper` | `M5PaperS3` | `M5PaperColor` | `M5PaperMono` |
| board | `m5stack-fire` | `esp32-s3-devkitm-1` | `esp32s3box` | `esp32-s3-devkitm-1` |
| 画面 | 540×960 / 16階調 | 540×960 / 16階調 | 400×600 / 6色 | 480×800 / 4階調 |
| パネル | IT8951 | ED047TC1 | ED2208 (Spectra 6) | SSD1677 |
| タッチ | GT911 | GT911 | **非搭載** | FT6336G |
| ボタン | A/B/C（ロータリ） | 電源のみ | A/B/C | A/B |
| フロントライト | – | – | – | あり |
| NFC | – | – | – | ST25R3916 |
| フック | – | **あり** | – | – |

機種ごとに次の点に注意する。

- **M5PaperColor はタッチパネルを搭載していない**。物理ボタンだけで完結する操作を必ず用意する。UI を変更したら、この機種で操作できるかを最初に確認する。
- **M5PaperColor は 1 画面のリフレッシュに 15〜30 秒かかる**。描画後にスリープや電源断を行う場合は `M5.Display.waitDisplay()` で完了を待つ。
- **電子ペーパーは `endWrite()` のたびに画面を更新する**。M5GFX は `Panel_FrameBufferBase::init()` で `_auto_display` を有効にするため、`println()` や `fillRect()` の 1 回ごとに更新が走る。複数の描画は必ず `startWrite()` / `endWrite()` で囲んで 1 回にまとめること。とくに M5PaperColor のパネル（ED2208）は部分更新を持たず毎回全面を転送するので、行単位の描き直しでは速くならない。更新の「回数」を減らすことだけが効く
- **`startWrite()` を SD アクセスをまたいで保持しない**。バスを共有する機種で競合しうる。進捗表示より、処理を終えてから 1 回で描くことを優先する
- **M5PaperS3 は筐体上部にフックがある**。掛けると上下が逆になるため、画像だけ 180 度回転させて表示する。他機種は回転させない。
- **M5PaperColor / M5PaperMono は OPI-PSRAM が必須**。`board_build.arduino.memory_type = qio_opi` が無いと M5GFX がディスプレイを初期化せず、画面が出ない。
- **M5Paper v1.1 だけ ESP32**（他は ESP32-S3）。S3 前提のコードを書かない。

M5PaperColor / M5PaperMono は M5GFX 0.2.20 以降でないと `board_t` に定義が無く、機種として認識されない。ライブラリを下げないこと。

## ディレクトリ構成

```
src/main.cpp     起動と画面の組み立てだけ。機種ごとの分岐はここに書かない
lib/
  DeviceProfile/ 機種差分の集約。新機種対応はまずここから
  ImageFile/     ファイル名の判定と EXIF の解析
  Menu/          一覧の表示と選択。タッチと物理ボタンの両対応
    Selection/   選択位置とページングの計算（純粋ロジック）
  M5Helper/      画像とテキストの描画
  Pressable/     タッチ領域
    Rect/        矩形の判定（純粋ロジック）
    Option/      描画オプション
test/            native 環境の単体テスト
```

## 設計ルール

### 機種差分は `DeviceProfile` に集約する

`if (board == ...)` を `src/` や他の `lib/` に書かない。差分は `DeviceProfile` のフィールドとして表現し、呼び出し側はプロファイルを読むだけにする。「M5PaperS3 だから反転する」ではなく「`profile.imageRotation` に従う」と書く。

### 純粋ロジックは `#ifdef ARDUINO` の外に置く

実機がなくても検証できる範囲を最大化するための、このリポジトリの中心的なルール。

- Arduino / M5Unified に依存しない計算・判定は、`#ifdef ARDUINO` の外に書いて native テストの対象にする
- 実機 API を叩く部分は `#ifdef ARDUINO` ... `#endif` で囲む
- ヘッダで `String` を使う場合は、既存のファイルと同じく native では `std::string` に読み替える

```cpp
#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
using String = std::string;
#endif
```

`DeviceProfile.h` / `ImageFile.h` / `Selection.h` / `Rect.h` がこの形になっている。

### コメントは日本語で書く

なぜそうしているかを書く。何をしているかはコードを読めば分かる。ハードウェア由来の理由（フック、リフレッシュ時間、タッチの有無など）は必ず残す。

## ビルドと書き込み

PlatformIO CLI は `~/.platformio/penv/bin/pio` にある。

```bash
# テスト（実機不要）
pio test -e native

# ビルド
pio run -e M5PaperS3
pio run -e M5Paper -e M5PaperS3 -e M5PaperColor -e M5PaperMono

# 書き込みとログ
pio run -e M5PaperS3 -t upload
pio device monitor -e M5PaperS3
```

`default_envs` は `M5PaperS3`。env を省略するとこれが対象になる。

## テスト

- **CI で回すのは native テストのみ**。`Arduino.h` や `M5Unified.h` を必要とするコードは CI でビルドはできてもテストはできない
- テストは `test/<スイート名>/` に置く。各ファイルが `main()` を持つため、`test/` 直下に複数のファイルを並べるとリンクに失敗する
- 機種差分を足したら `test/test_device_profile/` に期待値を書く。実機がなくても取り違えに気付ける

実機でしか確認できないこと（描画結果、タッチ、スリープ復帰、SD の読み書き）は、対象の機種すべてで手動確認する。特に UI を変えたときは M5PaperColor でのボタン操作を必ず確認する。

## 新しい機種を追加する手順

1. M5GFX の `src/lgfx/v1/boards.hpp` に `board_t` の定義があるか確認する。無ければライブラリの更新が必要
2. M5GFX の `src/M5GFX.cpp` でパネルとタッチの初期化を読み、解像度・階調・タッチの有無を確認する
3. M5Unified の `src/M5Unified.cpp` の `_pin_table_sd` に SD のピン定義があるか、ボタンが何個割り当たっているかを確認する
4. `platformio.ini` に env を追加する（`extends = device` で共通設定を継承し、`board` と `-DCARDCASE_FALLBACK_BOARD` を指定する）
5. `DeviceProfile.h` の `DeviceKind` と `DeviceProfile.cpp` の `profileFor()` / `currentDeviceKind()` に追加する
6. `test/test_device_profile/` に期待値を追加する
7. `.github/workflows/ci.yml` の matrix に env を追加する
8. `AGENTS.md` と `README.md` の機種表を更新する
9. 実機で確認する

## ブランチとコミット

- `main` に直接コミットしない。`feature/...` などのブランチを切る
- 意味のある単位でこまめにコミットする。コミットメッセージは日本語で、何を変えたかに加えてなぜそうしたかを書く
- 変更は PR にまとめる。CI（native テスト + 全機種ビルド）が通ってからマージする
- 大きな機能は PR を分ける。実機確認が必要な変更は、確認できる単位で区切る

## 今後の予定

- **WiFi 経由の画像転送**: 本体が SoftAP を立て、画面に接続用 QR コードを表示する。スマホのブラウザから画像をアップロードして SD に保存する。リサイズと階調変換はブラウザの canvas 側で行い、本体は保存するだけにする

  M5PaperColor ではこれを**転送手段ではなく選択手段**として位置づける。本体の一覧を歩くと 1 手ごとに全面更新（十数秒）が発生するが、スマホで選んで送れば本体の更新は表示の 1 回だけで済む。この機種では web 経由を推奨の導線とし、本体の一覧は SD だけで完結させたいときの手段に留める。

- **NFC タグでの画像切り替え**（M5PaperMono のみ）: NFC タグに書いたファイル名を読んで、対応する画像に切り替える
