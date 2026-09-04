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
- **`startWrite()` を SD アクセスをまたいで保持しない**。M5Paper は EPD（IT8951）と SD が SPI を共有しており、バスを保持したまま `SD.open()` を呼ぶとそこで固まる。進捗表示より、処理を終えてから 1 回で描くことを優先する

  LGFX には共有バスへの対処があるが（`DataWrapper::need_transaction` を見て読み出しのたびにバスを解放する）、効くのは**画像のデコード中の読み出しだけ**で、こちらが呼ぶ `open` / `close` は対象外になる。ファイルの開閉は `startWrite()` の外で行うこと
- **QR のような細かい模様のあとは残像が残る**（M5Paper v1.1）。画質優先の波形で塗り直しても消え切らず、次に描くものの下に透ける。一度黒で塗って粒子を反転させてから白に戻す必要がある。更新が 2 回増えるので `DeviceProfile` の `needsExtraClear` で必要な機種だけに限っている
- **画像の表示は `epd_quality` を使う**。M5PaperMono で点滅を減らそうと `epd_fast` / `epd_text` を試したが、`epd_fast` は前の画面が残り、`epd_text` は `epd_quality` と体感が変わらなかった。点滅と残像は表裏なので、画質を取る
- **スリープからの復帰は電子ペーパーが大きく点滅する**。`M5.begin()` が `Panel_SSD1677_4Gray::init()` などでコントローラをリセットし、パネルの状態を既知にするために更新をかけるため、アプリ側からは減らせない。画像を表示したあとすぐスリープせず、しばらく起きたままボタンで一覧に戻れるようにしてあるのはこのため（`VIEWING_TIMEOUT_MS`）
- **M5PaperS3 は筐体上部にフックがある**。掛けると上下が逆になるため、画像だけ 180 度回転させて表示する。他機種は回転させない。
- **M5PaperColor / M5PaperMono は OPI-PSRAM が必須**。`board_build.arduino.memory_type = qio_opi` が無いと M5GFX がディスプレイを初期化せず、画面が出ない。
- **M5Paper v1.1 だけ ESP32**（他は ESP32-S3）。S3 前提のコードを書かない。
- **M5PaperMono の microSD は 4bit の SDMMC 配線**（他機種は SPI）。M5Unified の `_pin_table_sd` で D1 / D2 が定義されているのがその機種にあたる。さらに **SD の各ラインに内部プルアップが要る**。Arduino の `SD_MMC` はスロットの `flags` を 0 のままにしていて内部プルアップを有効にしないため、そのままではカードが CMD に応答せず `send_op_cond` がタイムアウトする。`lib/Storage` がこの差分を吸収しているので、SD を直接触らずそちら経由で読むこと
- **SD のピン番号を計算で作らない**。`M5.getPin()` から 1 本ずつ受け取る。「D1 は D0 の隣だろう」と導出すると、SPI の機種では D1 / D2 が未接続でその番号が別の用途（M5PaperS3 なら SCLK と MOSI、M5PaperColor なら EPD RST）に使われているため、無関係なピンを掴む。`test/test_sd_pins/` に 4 機種の実際のピンテーブルを置いて担保している
- **画面の端まで描かない**。筐体のベゼルが縁に掛かるので文字が読めなくなる。余白は `DeviceProfile` の `margin()` から取る。固定値だと幅の狭い機種（M5PaperColor は 400px）で相対的に足りなくなる
- **LGFX は改行するとカーソルの X が 0 に戻る**。複数行を余白の位置から描くには、行ごとに `setCursor` してから改行なしで出す。`setCursor` が効くのは最初の 1 行だけ
- **文字は幅に収まる大きさを実行時に選ぶ**。機種名の長さも画面の幅も機種ごとに違うので、固定のサイズだとどこかで折り返すか切れる。見出しと操作ガイドがこの形になっている
- **同じ BtnA でも機種によって物理的な位置が違う**。M5Paper v1.1 はロータリスイッチの 上 / 押込 / 下 が BtnA / BtnB / BtnC に並ぶが、M5PaperColor は上下ボタンが BtnA / BtnB に隣り合い、3 つ目が BtnC になる。割り当ては `DeviceProfile` の `buttonA` / `buttonB` / `buttonC` に役割として持たせ、コード側でボタン名に意味を持たせない

M5PaperColor / M5PaperMono は M5GFX 0.2.20 以降でないと `board_t` に定義が無く、機種として認識されない。ライブラリを下げないこと。

## ディレクトリ構成

```
src/main.cpp     起動と画面の組み立てだけ。機種ごとの分岐はここに書かない
lib/
  DeviceProfile/ 機種差分の集約。新機種対応はまずここから
  ImageFile/     ファイル名の判定、EXIF と画像寸法の解析、表示する向きの決定
  Menu/          一覧の表示と選択。タッチと物理ボタンの両対応
  Storage/       microSD へのアクセス。SPI と SDMMC の差を吸収する
    SdPins/      配線の判定（純粋ロジック）
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

## 実機のログを見る

画面に何も出ないなど、実機でしか分からない不具合はシリアルログが頼りになる。ログのフラグは `platformio.ini` に入れず、必要なときだけ環境変数で足す。

```bash
PLATFORMIO_BUILD_FLAGS="-DARDUINO_USB_CDC_ON_BOOT=1 -DCORE_DEBUG_LEVEL=5" \
  pio run -e M5PaperMono -t upload
```

`ARDUINO_USB_CDC_ON_BOOT` が無いとネイティブ USB にシリアルが出ない。`CORE_DEBUG_LEVEL=5` にすると M5GFX の `[Autodetect]` や各ドライバの検出結果まで見える。機種判定が通っているか、パネルやタッチが見つかっているかがここで分かる。

`pio device monitor` は TTY を要求するので、対話端末以外からは使えない。その場合は pyserial で直接読む。

## リリース

`.github/workflows/release.yml` が機種ごとのファームウェアを作り、GitHub Release に添付する。

- **タグ**を打つと走る。タグ名はバージョンそのもの（例: `1.2.3`）
- Actions の画面から**手動実行**もできる（バージョンを入力する）

リリースノートは `--generate-notes` で、前回のリリース以降にマージされた PR が自動で列挙される。書き込み方法の説明はその上に置かれる。

成果物は機種ごとに 2 つ。

- `<機種>-merged.bin` … ブートローダ・パーティション・boot_app0・アプリを 1 つにまとめたもの。`write_flash 0x0` だけで書き込める
- `<機種>-firmware.bin` … アプリ部分のみ（offset は 0x10000）

**M5Paper v1.1 だけブートローダの位置が 0x1000**（他は 0x0）。ESP32 と ESP32-S3 の違いなので、機種を追加するときは `release.yml` の matrix に chip と bootloader_offset を書くこと。

### ブラウザから書き込めるページ

`docs/` に ESP Web Tools のページと機種ごとの manifest を置いてあり、リリース時に GitHub Pages へ公開する。ページと manifest はリポジトリで管理し、統合バイナリだけをその場のビルドから並べる。

**バージョンは公開時に差し替える。** manifest は `version` に `0.0.0` を、ページは `%VERSION%` を置いておき、`release.yml` の `pages` ジョブで今回のリリースの値にする。ページ側は置き換わっていなければ非表示にするので、手元で開いてもプレースホルダは見えない。

**機種は自動判別されない。** ESP Web Tools はチップを見て選ぶが、判別できるのは `chipFamily` 単位で、M5PaperS3 / M5PaperColor / M5PaperMono はいずれも ESP32-S3 になる。ページには機種ごとのボタンを並べ、取り違えないよう外観の違いを添えている。機種を追加したら `docs/index.html` にボタンを、`docs/` に manifest を足すこと。

Web Serial を使うのでパソコンの Chrome / Edge 専用。iOS は非対応、Android も未実装。

### Actions の指定はコミットハッシュで固定する

`uses:` はタグではなくコミットハッシュで指定し、読めるようにバージョンをコメントで添える。タグは付け替えられるため、供給元が変わると気付けない。

```yaml
uses: actions/checkout@11d5960a326750d5838078e36cf38b85af677262 # v4.4.0
```

更新するときはハッシュとコメントの両方を直す。

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

## WiFi での画像の受け取り

`lib/WebTransfer` が本体をアクセスポイントにして、スマホのブラウザから画像を受け取る。

- SSID とパスワードは **MAC から作り、毎回同じ値にする**。起動のたびに変えるとスマホが覚えた古いパスワードで接続に失敗し、ネットワーク設定の削除が要る。画面に QR とあわせて出すので秘匿は目的にしていない
- `DNSServer` ですべての問い合わせを自分に向け、キャプティブポータルとして振る舞う。繋いだ時点でスマホがアップロード画面を開く
- **縮小と EXIF の反映はブラウザ側で行う**。本体は保存と表示だけ。縮小先は本体が実際に使う表示領域に合わせる（横長の画像なら縦横を入れ替えた枠）。**回転はブラウザ側で焼き込まない** — 機種ごとに違う回転方向を二重に持つことになるため、本体側の判断に任せる
- **減色はしない**。パネル側が `epd_mode` に応じてディザリングする（M5PaperColor の `Panel_ED2208` は `epd_quality` で誤差拡散）
- SD があるときは書き込みながら受け、無いときだけメモリに溜める。**SD が無くても受け取って表示できる**。M5Paper v1.1 は ESP32 で PSRAM を 4MB しかマップできず、そこから画面のフレームバッファと WiFi スタックが取るため大きくは確保できない。取れるところまで小さくしながら試し、**確保できた量をブラウザに伝えて、その中に収まるまで圧縮させる**
- 受け取った画像は `WiFi.png`（または `.jpg`）として保存し、送るたびに上書きする。形式が変わると別の名前になって両方残るので、保存前にまとめて消す

### スマホの接続画面には制約がある

WiFi に繋いだ直後に自動で開く画面は、OS が用意した接続専用の簡易ブラウザで、できることが限られる。**アプリ側では直せないので、案内を出して回避する。**

| OS | 制約 |
| --- | --- |
| iOS | カメラを起動できない。選ぶとシートごと閉じる。写真ライブラリからは選べる |
| Android | ファイル選択そのものが動かない。ブラウザで `192.168.4.1` を開いてもらう |

iOS はこの画面を離れると、インターネットの無いネットワークから元の WiFi へ自動で戻ってしまう。そのため「閉じてからブラウザで開き直す」案内は iOS では成立しない（モバイルネットワークを切れば繋がる）。Android は接続を保つので成立する。**案内は `navigator.userAgent` で出し分けている。**

## NFC での画像の受け取り（M5PaperMono のみ）

`lib/NfcTransfer` が本体をタグとして振る舞わせ、スマホのアプリから画像を受け取る。

**ブラウザからは扱えない。** iOS には NFC の API が無く、Android の Web NFC も NDEF しか扱えないため、専用のアプリが要る。画像はブラウザから送れる WiFi のほうが手軽なので、NFC はオフラインで完結させたいときの手段として位置づける。

- NFC の電源は IO エキスパンダの `gpio4`。本体を再起動してもエキスパンダの状態は残るので、**一度落としてから上げ直す**
- **割り込みは使わずレジスタを読みに行く**。受け身の状態へ移るときに割り込みの端子に頼ると取りこぼす
- エミュレーションできるのは **NTAG2 系か MIFARE Ultralight** だけ。種別と UID を指定しないと `Not support` で弾かれる。UID は MAC から作り、同じ本体なら毎回同じ値にする
- **ライブラリの受信バッファは 64 バイト固定**で、こちらの 253 バイトのフレームが入らない。`scripts/patch_m5unit_nfc.py` がビルド前に広げる。あわせて、長さを確かめずにバッファへ読み込む箇所も塞いでいる（超えるとメモリを壊すため）。ライブラリの更新で対象の記述が見つからなくなったらビルドを止める

**送る形式はまず PNG を試し、収まらなければ JPEG にする。** WiFi 側と同じ判断。名刺のような文字や図の画像は PNG のほうが小さくなる上に劣化しない。写真だけが PNG では大きくなるので、そこで JPEG に落ちる。

受け取った画像は `NFC.jpg`（または `.png`）として保存し、送るたびに上書きする。形式は中身から判断する（名前が付いてこないため）。拡張子が変わると別の名前になって両方残るので、保存前にまとめて消す。SD が無ければ表示するだけ。

**フレームごとにログを出さないこと。** 1 往復ごとに時間が増えて転送が目に見えて遅くなる。調べるときだけ一時的に入れる。

やり取りの決まりは `lib/NfcTransfer/Protocol` と `app/src/protocol.ts` の**両方に同じ内容がある**。どちらかを変えたらもう一方も直すこと。とくに CRC-32 は値が食い違うと転送が必ず失敗する。

仕様は [docs/nfc-protocol.md](docs/nfc-protocol.md) にまとめてある。**この文書は他の言語で送る側を実装するための唯一の手がかりになる**ので、決まりを変えたら必ず更新すること。文書に載せているバイト列は `test/test_nfc_protocol/` で実装と突き合わせているので、食い違えば CI で落ちる。

## スマホのアプリ（app/）

Expo で作った React Native のアプリ。NFC で画像を送る。

```bash
cd app
npm install
npx expo run:ios --device   # 実機に入れる
```

**NFC は実機でしか動かない。** シミュレータでは試せないので、必ず端末を繋いで確認する。Apple Developer Program（有料）も要る。NFC Tag Reading の capability は無料のプロビジョニングでは有効にできない。

開発中はローカルビルド（`expo run:ios`）を使う。

### リリース用のビルドは EAS で焼く

特定の Mac に依存させないため。手元で焼くと、その機械の Xcode・鍵・`ios/` の状態が成果物に混ざる。実際、`app.json` で bundle id を `net.mituwa.cardcase` に変えたのに `ios/` が古いままで、`com.mitsuharu.cardcase` のまま焼けていたことがある。**`expo run:ios` は `ios/` が既にあると `prebuild` を走らせない。**EAS は毎回まっさらな状態で `app.json` から作り直すので、これが起きない。

リリースは多くないので無料枠で足りる。**開発中は手元の `expo run:ios` を使う。**キューを待っていられないため。

配るときはタグを打つ。[App Release](.github/workflows/app-release.yml) が EAS に投げ、出来上がりを DeployGate に上げる。

```bash
git tag app-1.2.3 && git push origin app-1.2.3
```

**タグは `app-` で始める。**ファームウェアのリリース（`1.2.3`）と同じタグで動かさないため。

証明書も Android の署名鍵も **EAS が預かる**。手元にもリポジトリにも鍵を置かない。版番号も EAS が数える（`eas.json` の `appVersionSource: remote` と `autoIncrement`）。

**鍵は初回に手元で対話的に作る。**`npx eas-cli build --profile release --platform all` を一度手で走らせる。CI は非対話なので、鍵が無い状態から始めると落ちる。

iOS の Ad Hoc は登録済みの端末にしか入らない。**端末を増やしたら `eas device:create` で登録してから焼き直す。**配布済みの ipa に後から端末は足せない。

## 今後の予定

- **M5PaperColor では WiFi を推奨の導線にする**: 本体の一覧を歩くと 1 手ごとに全面更新（十数秒）が発生する。スマホで選んで送れば本体の更新は表示の 1 回だけで済むので、この機種では WiFi 経由を主な導線として案内する
- **NFC タグでの画像切り替え**（M5PaperMono のみ）: 市販の NFC タグに書いたファイル名を読んで、対応する画像に切り替える。いまはタグとして振る舞う側だけを実装している
