import { useCallback, useEffect, useRef, useState } from 'react'
import { Pressable, StyleSheet, Text, TextInput, View } from 'react-native'
import { WebView } from 'react-native-webview'

import { bytesFromDataUrl } from './base64'
import { CloseAccessory, closeAccessoryId } from './CloseAccessory'
import { pickImage, takePhoto, toDataUrl, type PreparedImage } from './imagePicker'
import { TEXT_RENDER_HTML, type CardRequest, type DrawResult } from './textRender'

export type CardResult =
  | { kind: 'empty' }
  /** 枠に対して文字が多すぎて、どの大きさでも収まらない */
  | { kind: 'overflow' }
  /** QR にするには URL が長すぎる */
  | { kind: 'url-too-long' }
  | { kind: 'ready'; image: PreparedImage; size: number }

type Props = {
  /** 本体の画面。はじめの枠の大きさになる */
  screenWidth: number
  screenHeight: number
  /** 送る大きさの目安。名刺に写真を載せると PNG では収まらないため */
  maxBytes: number
  onResult: (result: CardResult) => void
  onError: (message: string) => void
}

// 名刺に載せる画像を縮める先。
// 画面の長い方に合わせておけば、名刺の中で使う大きさには足りる。
const IMAGE_BOX = 800

// スライダーは RN の標準に無く、そのために依存を足すほどのものでもない。
// TextComposer と同じ段にしてある。
const SHARES = [
  { value: 1, label: '自動' },
  { value: 0.8, label: '80%' },
  { value: 0.6, label: '60%' },
  { value: 0.4, label: '40%' },
]

// キーボードの上に出す「閉じる」を付ける入力欄。
// 欄ごとに別の名前が要る。理由は CloseAccessory.tsx にある。
const FIELDS = ['card-title', 'card-subtitle', 'card-account', 'card-url', 'card-w', 'card-h']

/** 枠の大きさ。極端な値は描く前に落とす。 */
function size(input: string, fallback: number): number {
  const value = Math.round(Number(input))
  if (!isFinite(value) || value < 16) {
    return fallback
  }
  return Math.min(value, 2000)
}

/**
 * 決まった項目を並べて名刺にする。
 *
 * 書いた文字をそのまま画像にする TextComposer と違い、こちらは項目
 * （画像・タイトル・サブタイトル・アカウント・QR）を受け取って組む。
 * 空にした項目は場所を取らず、残りが詰まって真ん中に来る。
 *
 * 描くのは WebView の canvas で、コードは本体の WiFi 画面と同じものを使う。
 * 詳しくは textRender.ts を見ること。
 */
export function CardComposer({ screenWidth, screenHeight, maxBytes, onResult, onError }: Props) {
  const [image, setImage] = useState<string | null>(null)
  const [title, setTitle] = useState('')
  const [subtitle, setSubtitle] = useState('')
  const [account, setAccount] = useState('')
  const [url, setUrl] = useState('')
  const [width, setWidth] = useState(String(screenWidth))
  const [height, setHeight] = useState(String(screenHeight))
  const [share, setShare] = useState(1)
  const [busy, setBusy] = useState(false)

  // 呼び出し側が毎回作り直す関数を渡してきても、描き直しの合図が
  // 増えないようにする。TextComposer と同じ理由。
  const callbacks = useRef({ onResult, onError })
  callbacks.current = { onResult, onError }

  const webview = useRef<WebView>(null)
  const ready = useRef(false)
  const waiting = useRef<CardRequest | null>(null)
  const latest = useRef(0)

  const draw = useCallback(() => {
    latest.current += 1
    const request: CardRequest = {
      id: latest.current,
      image,
      title,
      subtitle,
      account,
      url: url.trim(),
      share,
      width: size(width, screenWidth),
      height: size(height, screenHeight),
      maxBytes,
    }

    const empty = request.image === null && request.url === ''
      && request.title.trim() === ''
      && request.subtitle.trim() === ''
      && request.account.trim() === ''
    if (empty) {
      callbacks.current.onResult({ kind: 'empty' })
      return
    }

    if (!ready.current) {
      waiting.current = request
      return
    }
    webview.current?.injectJavaScript(`window.drawCard(${JSON.stringify(request)}); true;`)
  }, [account, height, image, maxBytes, screenHeight, screenWidth, share, subtitle, title, url, width])

  // 1 文字ごとに描き直すと、そのたびに二分探索と PNG の生成が走る。
  // 手が止まってからにする。
  useEffect(() => {
    const timer = setTimeout(draw, 300)
    return () => clearTimeout(timer)
  }, [draw])

  async function choose(source: 'library' | 'camera') {
    try {
      setBusy(true)
      const picked = source === 'library' ? await pickImage() : await takePhoto()
      if (!picked) {
        return
      }
      setImage(await toDataUrl(picked, IMAGE_BOX))
    } catch (e) {
      callbacks.current.onError(e instanceof Error ? e.message : '画像を読み込めませんでした')
    } finally {
      setBusy(false)
    }
  }

  function receive(raw: string) {
    let result: DrawResult
    try {
      result = JSON.parse(raw) as DrawResult
    } catch {
      callbacks.current.onError('名刺を画像にできませんでした')
      return
    }

    if (result.ready) {
      ready.current = true
      if (waiting.current) {
        const request = waiting.current
        waiting.current = null
        webview.current?.injectJavaScript(`window.drawCard(${JSON.stringify(request)}); true;`)
      }
      return
    }

    // 追い抜かれた結果は捨てる。古い絵を送ってしまわないため。
    if (result.id !== latest.current) {
      return
    }

    if (result.tooLong) {
      callbacks.current.onResult({ kind: 'url-too-long' })
      return
    }

    if (result.error) {
      callbacks.current.onError(result.error)
      return
    }

    // 枠に入らなかったときは絵が返らない。送らせない。
    if (!result.drawn || !result.dataUrl) {
      callbacks.current.onResult({ kind: 'overflow' })
      return
    }

    try {
      callbacks.current.onResult({
        kind: 'ready',
        size: result.size ?? 0,
        image: {
          bytes: bytesFromDataUrl(result.dataUrl),
          uri: result.dataUrl,
          width: result.width ?? 0,
          height: result.height ?? 0,
        },
      })
    } catch (e) {
      callbacks.current.onError(e instanceof Error ? e.message : '名刺を画像にできませんでした')
    }
  }

  return (
    <View>
      <View style={styles.row}>
        <Pressable
          style={[styles.chip, styles.grow, busy && styles.disabled]}
          onPress={() => choose('library')}
          disabled={busy}>
          <Text style={styles.chipLabel}>写真から選ぶ</Text>
        </Pressable>
        <Pressable
          style={[styles.chip, styles.grow, busy && styles.disabled]}
          onPress={() => choose('camera')}
          disabled={busy}>
          <Text style={styles.chipLabel}>撮影する</Text>
        </Pressable>
        {image !== null && (
          <Pressable style={styles.chip} onPress={() => setImage(null)}>
            <Text style={styles.chipLabel}>外す</Text>
          </Pressable>
        )}
      </View>

      <View style={styles.field}>
        <Text style={styles.caption}>タイトル</Text>
        <TextInput
          style={styles.input}
          value={title}
          onChangeText={setTitle}
          placeholder="山田 太郎"
          placeholderTextColor="#999"
          inputAccessoryViewID={closeAccessoryId('card-title')}
        />
      </View>

      <View style={styles.field}>
        <Text style={styles.caption}>サブタイトル</Text>
        <TextInput
          style={styles.input}
          value={subtitle}
          onChangeText={setSubtitle}
          placeholder="Taro Yamada"
          placeholderTextColor="#999"
          inputAccessoryViewID={closeAccessoryId('card-subtitle')}
        />
      </View>

      <View style={styles.field}>
        <Text style={styles.caption}>SNS アカウント</Text>
        <TextInput
          style={styles.input}
          value={account}
          onChangeText={setAccount}
          placeholder="@example"
          placeholderTextColor="#999"
          autoCapitalize="none"
          inputAccessoryViewID={closeAccessoryId('card-account')}
        />
      </View>

      <View style={styles.field}>
        <Text style={styles.caption}>QR にする URL</Text>
        <TextInput
          style={styles.input}
          value={url}
          onChangeText={setUrl}
          placeholder="https://example.com/"
          placeholderTextColor="#999"
          autoCapitalize="none"
          autoCorrect={false}
          keyboardType="url"
          inputAccessoryViewID={closeAccessoryId('card-url')}
        />
      </View>

      <View style={styles.row}>
        <View style={styles.number}>
          <Text style={styles.caption}>幅</Text>
          <TextInput
            style={styles.input}
            value={width}
            onChangeText={setWidth}
            keyboardType="number-pad"
            inputAccessoryViewID={closeAccessoryId('card-w')}
          />
        </View>
        <View style={styles.number}>
          <Text style={styles.caption}>高さ</Text>
          <TextInput
            style={styles.input}
            value={height}
            onChangeText={setHeight}
            keyboardType="number-pad"
            inputAccessoryViewID={closeAccessoryId('card-h')}
          />
        </View>
        <Pressable
          style={styles.chip}
          onPress={() => {
            setWidth(height)
            setHeight(width)
          }}>
          <Text style={styles.chipLabel}>縦横を入れ替え</Text>
        </Pressable>
      </View>

      <View style={styles.row}>
        <Text style={styles.caption}>文字の大きさ</Text>
        {SHARES.map((option) => (
          <Pressable
            key={option.value}
            style={[styles.chip, share === option.value && styles.chipOn]}
            onPress={() => setShare(option.value)}>
            <Text style={[styles.chipLabel, share === option.value && styles.chipLabelOn]}>
              {option.label}
            </Text>
          </Pressable>
        ))}
      </View>

      <Text style={styles.hint}>
        空にした項目は詰めて並べます。横長にすると画像を左、文字と QR を右に置きます。
      </Text>

      {FIELDS.map((name) => (
        <CloseAccessory key={name} name={name} />
      ))}

      <View style={styles.hidden} pointerEvents="none">
        <WebView
          ref={webview}
          source={{ html: TEXT_RENDER_HTML }}
          originWhitelist={['*']}
          javaScriptEnabled
          onMessage={(event) => receive(event.nativeEvent.data)}
        />
      </View>
    </View>
  )
}

const styles = StyleSheet.create({
  row: { flexDirection: 'row', alignItems: 'flex-end', gap: 8, marginTop: 12 },
  grow: { flex: 1, alignItems: 'center' },
  field: { marginTop: 12 },
  number: { flex: 1 },
  caption: { fontSize: 13, color: '#555', marginBottom: 4 },
  input: {
    paddingVertical: 10,
    paddingHorizontal: 12,
    fontSize: 15,
    color: '#222',
    borderWidth: 1,
    borderColor: '#bbb',
    borderRadius: 6,
  },
  chip: {
    paddingVertical: 10,
    paddingHorizontal: 12,
    borderWidth: 1,
    borderColor: '#1257a0',
    borderRadius: 6,
  },
  chipOn: { backgroundColor: '#1257a0' },
  chipLabel: { fontSize: 13, fontWeight: '600', color: '#1257a0' },
  chipLabelOn: { color: '#fff' },
  disabled: { opacity: 0.5 },
  hint: { marginTop: 10, fontSize: 12, color: '#777', lineHeight: 18 },
  // 絵を作るためだけの WebView。見せる必要はないが、
  // 大きさを 0 にすると端末によっては動かないので 1px 残す。
  hidden: { position: 'absolute', width: 1, height: 1, opacity: 0 },
})
