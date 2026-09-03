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
3. 一覧からファイルを選ぶと全画面で表示し、そのままスリープに入ります
4. 選び直すときは電源ボタンを押します

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

## Development

開発ルールは [AGENTS.md](AGENTS.md) にまとめています。

```bash
# 実機不要の単体テスト
pio test -e native
```

## License

[MIT](LICENSE)
