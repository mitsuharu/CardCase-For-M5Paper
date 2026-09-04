# CardCase App

M5PaperMono に NFC で画像を送るアプリです。Expo で作った React Native のアプリで、iOS と Android の両方で動きます。

**NFC は実機でしか動きません。** シミュレータやエミュレータでは試せないので、必ず端末を繋いで確認してください。

## 必要なもの

| | 内容 |
| --- | --- |
| 共通 | Node.js 20 以上 |
| iOS | Xcode、**Apple Developer Program（有料）** |
| Android | Android Studio |

iOS で有料アカウントが要るのは、**NFC Tag Reading の capability が無料のプロビジョニングでは有効にできない**ためです。これは Swift で書いても同じです。

## 準備

```bash
cd app
npm install
```

### Android Studio の設定

Android Studio を入れたあと、次を済ませてください。

1. **SDK Manager** で `Android SDK Platform-Tools` と、対象の API レベルの SDK を入れる
2. 環境変数を通す（`~/.zshrc` などに追記）

```bash
export ANDROID_HOME="$HOME/Library/Android/sdk"
export PATH="$PATH:$ANDROID_HOME/platform-tools"
```

3. 端末側で**開発者向けオプション**と **USB デバッグ**を有効にする
4. 繋いで認識されるか確かめる

```bash
adb devices
```

## 開発

実機に入れて動かします。初回は `expo prebuild` が走ってネイティブのプロジェクトが作られます。

```bash
npm run ios       # または npx expo run:ios --device
npm run android   # または npx expo run:android --device
```

iOS は初回に署名の設定が要ります。Xcode の `Settings` → `Accounts` で Apple ID を登録し、`Manage Certificates` から `Apple Development` を追加してください。それでも通らない場合は Xcode で直接開いて Team を選びます。

```bash
open ios/CardCase.xcworkspace
```

**`ios/` と `android/` は git の管理外**です。恒久的な設定は `app.json` に書いてください。そちらに書いておけば `prebuild` をやり直しても引き継がれます。

## 配布

使うのが自分と身内だけなら、審査の要らない方法が楽です。

| | 形式 | 制約 |
| --- | --- | --- |
| iOS | Ad Hoc | **端末の UDID 登録**が必要。有効期限 1 年、最大 100 台 |
| Android | APK | 制約なし。提供元不明のアプリを許可するだけ |

EAS でビルドすると、`internal` の配布用に iOS は Ad Hoc、Android は APK が出ます。

```bash
npx eas build --profile preview --platform ios
npx eas build --profile preview --platform android
```

無料枠は iOS / Android それぞれ月 15 回・低優先度キューです。**直しては試す作業には向かない**ので、開発中は上のローカルビルドを使い、配布するときだけ使ってください。

できあがった成果物は DeployGate などに置いて配れます。

## 中身

| ファイル | 役割 |
| --- | --- |
| `src/protocol.ts` | やり取りの決まり。本体側の `lib/NfcTransfer/Protocol` と対 |
| `src/nfcSender.ts` | NFC の送信。iOS と Android の差を吸収する |
| `src/imagePicker.ts` | 画像の選択・縮小・形式の変換 |
| `App.tsx` | 画面 |

**`src/protocol.ts` を変えたら、本体側も直してください。** とくに CRC-32 は値が食い違うと転送が必ず失敗します。決まりは [NFC 転送プロトコル](../docs/nfc-protocol.md) にまとめてあります。

## 動かないとき

| 症状 | 見るところ |
| --- | --- |
| NFC のシートが出ない | entitlement が付いているか。`app.json` の `ios.entitlements` |
| かざしても反応しない | 本体で `[NFC]` を選んでいるか。iPhone は上端の裏、Android は背面中央あたりにアンテナがある |
| 途中で切れる | 正常。かざしたままにしていれば続きから再開する |
| 送信が始まらない | 本体側のシリアルログを見る。`nfc: listening` が出ているか |
