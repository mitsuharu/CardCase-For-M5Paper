# CardCase For M5Paper

M5Stack の電子ペーパー製品で、SD カードに保存した画像を選択して表示します。いろんな画像を保存して、シーンに合わせて画像を切り替えましょう。

## Demo

[![動作デモ動画](README_Images/thumbnail.png)](https://www.youtube.com/watch?v=dRgxxADyUGU)

## 対応機種

| 機種 | PlatformIO env | 画面 | 操作 |
| --- | --- | --- | --- |
| [M5Paper v1.1](https://docs.m5stack.com/ja/core/m5paper_v1.1) | `M5Paper` | 540 × 960 / 16 階調 | タッチ・ボタン |
| [M5PaperS3](https://docs.m5stack.com/ja/core/PaperS3) | `M5PaperS3` | 540 × 960 / 16 階調 | タッチ |
| [M5PaperColor](https://docs.m5stack.com/ja/core/PaperColor) | `M5PaperColor` | 400 × 600 / 6 色 | ボタン |
| [M5PaperMono](https://docs.m5stack.com/ja/core/PaperMono) | `M5PaperMono` | 480 × 800 / 4 階調 | タッチ・ボタン |

M5PaperS3 は筐体上部にフックがあり、掛けると上下が逆になるため、画像だけ 180 度回転して表示します。M5PaperColor はタッチパネルを搭載していないので、物理ボタンで操作します。

## Requirements

- 上記のいずれかの機種
- VS Code + PlatformIO

## Usage

1. 画像（jpeg または png）を SD カードのルートに保存します
   - 画面に収まるよう自動で縮小して中央に表示するので、サイズは厳密でなくても構いません
   - スマホやデジカメで撮った写真は EXIF の向きに従って回転します
   - 横長の写真は画面の向きに合わせて回転し、画面いっぱいに表示します
   - きれいに出したい場合は機種の画面サイズに合わせてください
   - 一覧に載るのは最大 32 枚です。1 画面に収まらない場合はページを送ります
2. SD カードを入れて電源を入れます
3. 一覧からファイルを選ぶと全画面で表示します
4. 選び直すときは、ボタンか画面のタッチで一覧に戻ります
5. しばらく操作がないとスリープに入ります。そのあとは電源ボタンで復帰します
   - 復帰では画面の初期化が入って電子ペーパーが大きく点滅します。続けて選び直すときは、スリープに入る前にボタンで戻るほうが速くてきれいです

## スマホから画像を送る

SD カードを抜き差ししなくても、スマホから直接画像を送れます。

1. 一覧から `[Receive by WiFi]` を選ぶ
2. 画面に出た QR コードをスマホのカメラで読む（本体のアクセスポイントに繋がります）
3. ブラウザが自動でアップロード画面を開く（開かない場合は `http://192.168.4.1`）
4. 画像を選んで送信すると、本体に表示されます

送る前にブラウザ側で画面の大きさに縮小するので、スマホで撮った大きな写真でもそのまま選べます。EXIF の向きも反映されます。

**SD カードが無くても使えます。** その場合は受け取った画像を表示するだけで、保存はしません。SD カードがあれば `WiFi.png` として保存し、次回から一覧に並びます。送るたびに上書きするので、カードが埋まることはありません。

アクセスポイントは受け取りが終わると自動で止まります。SSID とパスワードは本体ごとに決まっていて毎回同じなので、二度目からはスマホが覚えています。

## Build

```bash
# ビルド
pio run -e M5PaperS3

# 書き込み
pio run -e M5PaperS3 -t upload
```

`env` は上の対応機種の表を参照してください。省略した場合は `M5PaperS3` が対象になります。

### 書き込みでエラーになるとき

```
A fatal error occurred: This chip is ESP32, not ESP32-S3. Wrong --chip argument?
```

機種と `env` が食い違っています。`-e` を省略すると `M5PaperS3` が対象になるため、M5Paper v1.1 に書き込むときは `-e M5Paper` を明示してください。VS Code から操作する場合は、PlatformIO のサイドバーか下部のステータスバーで `env` を切り替えてから実行します。

## Install

### ブラウザから書き込む（かんたん）

パソコンの Chrome か Edge で [インストーラのページ](https://mitsuharu.github.io/CardCase-For-M5Paper/) を開き、本体を USB-C で繋いで、お使いの機種のボタンを押すだけです。何もインストールする必要はありません。

Web Serial を使うため、**パソコンの Chrome / Edge 専用**です。iPhone や Android では書き込めません。また機種は自動判別されないので、ボタンは自分で選んでください（M5PaperS3 / M5PaperColor / M5PaperMono はいずれも ESP32-S3 で、ブラウザからは区別がつきません）。

### コマンドで書き込む

ビルド済みのファームウェアは [Releases](../../releases) から入手できます。PlatformIO を用意しなくても書き込めます。

`<機種>-merged.bin` はブートローダとパーティションを含んだ統合バイナリで、offset を指定せずそのまま書き込めます。

```bash
pip install esptool
esptool.py --chip esp32s3 --port /dev/tty.usbmodem1101 write_flash 0x0 M5PaperS3-merged.bin
```

M5Paper v1.1 は ESP32 なので `--chip esp32` を指定してください。ポート名は環境に合わせて読み替えてください（macOS は `/dev/tty.usb*`、Windows は `COM3` など）。

## Development

開発ルールは [AGENTS.md](AGENTS.md) にまとめています。

```bash
# 実機不要の単体テスト
pio test -e native
```

## License

[MIT](LICENSE)
