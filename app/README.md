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

**手元で焼くのは開発中だけ**です。配るものは EAS で焼きます。理由は[配布](#配布)にあります。

## 配布

審査を通さずに配る。使うのが自分と身内だけなら、この方が早い。

**焼く場所は 2 つを使い分ける。**

| | どこで焼く | なぜ |
| --- | --- | --- |
| 開発中 | 手元（`expo run:ios`） | キューを待っていられない |
| リリース | EAS（GitHub Actions から） | 特定の Mac に依存させない |

手元で焼くと、その機械の Xcode・鍵・`ios/` の状態が成果物に混ざる。実際、`app.json` で bundle id を変えたのに `ios/` が古いままで、別の id のまま焼けていたことがある。EAS は毎回まっさらな状態で `app.json` から作り直すので、これが起きない。

リリースは多くないので無料枠（iOS / Android それぞれ月 15 回・低優先度キュー）で足りる。

**タグを打てば、焼いて配るところまで自動で終わる。**[App Release](../.github/workflows/app-release.yml) が EAS に投げ、出来上がりを配る。

### 配り口は 3 つ

| | iOS | Android | |
| --- | :-: | :-: | --- |
| EAS の配布ページ | ○ | ○ | 設定が要らない。ビルドが終わると QR 付きのページができる |
| DeployGate | ○ | ○ | 必須ではないが、他の案件と同じ場所にまとめたいので通している |
| GitHub Release | – | ○ | **誰でも入れられる。**端末の登録が要らない |

**iOS を GitHub Release に置かないのは、置いても誰も入れられないため。**Ad Hoc は登録済みの端末にしか入らず、プロファイルに端末の UDID が焼き込まれている。

| | 形式 | 制約 |
| --- | --- | --- |
| iOS | Ad Hoc | **端末の UDID 登録**が必要。有効期限 1 年、最大 100 台 |
| Android | APK | 制約なし。提供元不明のアプリを許可するだけ |

### 一度だけやること

**1. 端末を登録して、初回だけ手元から焼く。**

```bash
cd app
npx eas-cli login
npx eas-cli device:create
npx eas-cli build --profile release --platform all
```

Ad Hoc は登録済みの端末にしか入らない。`device:create` では Apple Developer に登録済みの端末を取り込める。

初回を手元でやるのは、**証明書と Android の署名鍵をここで作るため。**対話で訊かれる。一度作れば EAS が預かるので、以降の CI は非対話で通る。逆に、これを飛ばして CI から始めると鍵が無くて落ちる。

**2. GitHub に鍵を登録する。**リポジトリの Settings → Secrets and variables → Actions。

| 名前 | 取るところ |
| --- | --- |
| `EXPO_TOKEN` | [expo.dev の Access tokens](https://expo.dev/settings/access-tokens) |
| `DEPLOYGATE_USER` | DeployGate のユーザー名 |
| `DEPLOYGATE_API_TOKEN` | [DeployGate の設定ページ](https://deploygate.com/settings) |

### 配るとき

タグを打つ。**ファームウェアのリリース（`1.2.3`）とは接頭辞で分けている**ので、混ざらない。

```bash
git tag app-1.2.3
git push origin app-1.2.3
```

メモを添えたいときや片方だけ焼きたいときは、Actions の画面から **App Release** を手動で実行する。対象（`all` / `ios` / `android`）とメモを指定できる。

版番号は EAS が数えている（`eas.json` の `appVersionSource: remote` と `autoIncrement`）。手で上げなくていい。`app.json` の `version` は、人に見せる版として節目で上げる。

### 手元から焼くとき

CI を通さず試したいときは、同じことを手で実行する。

```bash
cd app
npx eas-cli build --profile release --platform ios
```

終わると配布ページの URL が出る。**そのまま QR で入れられる**ので、自分の端末で試すだけならここで終わり。

DeployGate にも置くなら、成果物を落としてから上げる。`build:download` はシミュレータ用のビルドを落とすコマンドなので、ipa / apk には使えない。

```bash
mkdir -p build
curl -sSL -o build/CardCase.ipa "$(npx eas-cli build:list --json --non-interactive --limit 1 --platform ios \
  | jq -r '.[0].artifacts.applicationArchiveUrl // .[0].artifacts.buildUrl')"
./scripts/upload-deploygate.sh build/CardCase.ipa "直した内容"
```

`upload-deploygate.sh` は `DEPLOYGATE_USER` と `DEPLOYGATE_API_TOKEN` を環境変数から読む。

### 端末を増やしたとき

UDID を登録してから、**焼き直す**。

```bash
npx eas-cli device:create
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
