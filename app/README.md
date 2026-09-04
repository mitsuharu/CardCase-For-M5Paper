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

審査を通さずに配る。使うのが自分と身内だけなら、この方が早い。

| | 形式 | 制約 |
| --- | --- | --- |
| iOS | Ad Hoc | **端末の UDID 登録**が必要。有効期限 1 年、最大 100 台 |
| Android | APK | 制約なし。提供元不明のアプリを許可するだけ |

**リリース用のビルドは EAS（クラウド）で焼く。**特定の Mac に依存させないため。手元でビルドすると、その機械の Xcode・鍵・`ios/` の状態が成果物に混ざる。実際、`app.json` で bundle id を変えたのに `ios/` が古いままで、別の id のまま焼けていたことがある。EAS は毎回まっさらな状態で `app.json` から作り直すので、これが起きない。

リリースは多くないので無料枠（iOS / Android それぞれ月 15 回・低優先度キュー）で足りる。**開発中は今までどおり手元の `expo run:ios` を使う。**キューを待っていられないため。

### 一度だけやること

```bash
cd app
npx eas login
npx eas build:configure
```

**iOS の端末を登録する。**Ad Hoc は登録済みの端末にしか入らない。

```bash
npx eas device:create
```

Apple Developer に登録済みの端末は取り込める。初回ビルド時に「どの端末を入れるか」を訊かれる。

証明書と Android の署名鍵は **EAS が作って預かる**ので、手元に鍵を置く必要はない。中身を見たいときは `npx eas credentials`。

**DeployGate に置くなら** API key を [設定ページ](https://deploygate.com/settings)で発行して、シェルの設定に書いておく。

```bash
export DEPLOYGATE_USER=あなたのユーザー名
export DEPLOYGATE_API_TOKEN=発行した API key
```

### 配るとき

```bash
cd app
npx eas build --profile release --platform all
```

終わると配布ページの URL が出る。**そのまま QR で入れられる**ので、身内に配るだけならここで終わり。

DeployGate に集約したいときは、成果物を落としてから上げる。

```bash
npx eas build:download --platform ios --latest --output build/CardCase.ipa
./scripts/upload-deploygate.sh build/CardCase.ipa "NFC の送信をやめられるようにした"
```

版番号は EAS が数えている（`eas.json` の `appVersionSource: remote` と `autoIncrement`）。手で上げなくていい。`app.json` の `version` は、人に見せる版として節目で上げる。

### 端末を増やしたとき

UDID を登録してから、**焼き直す**。

```bash
npx eas device:create
npx eas build --profile release --platform ios
```

配布済みの ipa に後から端末は足せない。プロファイルに焼き込まれているため。

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
