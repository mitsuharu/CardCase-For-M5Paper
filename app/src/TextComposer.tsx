import { useCallback, useEffect, useRef, useState } from 'react'
import {
  InputAccessoryView,
  Keyboard,
  Platform,
  Pressable,
  StyleSheet,
  Text,
  TextInput,
  View,
} from 'react-native'
import { WebView } from 'react-native-webview'

import { bytesFromDataUrl } from './base64'
import type { PreparedImage } from './imagePicker'
import { TEXT_RENDER_HTML, type DrawRequest, type DrawResult } from './textRender'

export type TextResult =
  | { kind: 'empty' }
  /** 枠に対して文字が多すぎて、どの大きさでも収まらない */
  | { kind: 'overflow' }
  | { kind: 'ready'; image: PreparedImage; size: number }

type Props = {
  /** 本体の画面。はじめの枠の大きさになる */
  screenWidth: number
  screenHeight: number
  onResult: (result: TextResult) => void
  onError: (message: string) => void
}

type Align = 'left' | 'center' | 'right'
type Reflow = 'auto' | 'keep'

// 折り返すと意味が変わる文字（表の見出しなど）のために、止められるようにしてある。
// 既定は自動。長い 1 行をそのまま入れようとすると読めない大きさになるため。
const REFLOWS: { value: Reflow; label: string }[] = [
  { value: 'auto', label: '自動' },
  { value: 'keep', label: 'しない' },
]

const ALIGNS: { value: Align; label: string }[] = [
  { value: 'left', label: '左' },
  { value: 'center', label: '中央' },
  { value: 'right', label: '右' },
]

// スライダーは RN の標準に無く、そのために依存を足すほどのものでもない。
// 段が 4 つあれば name card の調整には足りる。
const SHARES = [
  { value: 1, label: '自動' },
  { value: 0.8, label: '80%' },
  { value: 0.6, label: '60%' },
  { value: 0.4, label: '40%' },
]

// キーボードの上に出す「閉じる」。
//
// 幅と高さは number-pad で、iOS のこのキーボードには改行も完了も無い。
// 文字の入力欄も複数行なので、改行では閉じられない。どちらも自分では
// 閉じられないので、アクセサリを付ける。Android は戻るで閉じられる。
const ACCESSORY_ID = 'cardcase-text-input'
const accessoryFor = Platform.OS === 'ios' ? ACCESSORY_ID : undefined

/** 枠の大きさ。極端な値は描く前に落とす。 */
function size(input: string, fallback: number): number {
  const value = Math.round(Number(input))
  if (!isFinite(value) || value < 16) {
    return fallback
  }
  return Math.min(value, 2000)
}

/**
 * 文字を画像にする。
 *
 * 描くのは WebView の canvas で、コードは本体の WiFi 画面と同じものを使う。
 * 詳しくは textRender.ts を見ること。
 *
 * WebView は見せない。ここで要るのは絵を作ることだけで、操作は RN 側で作る。
 * 画面に出すと、端末の幅に合わせて縮んだものを見せることになってしまう。
 */
export function TextComposer({ screenWidth, screenHeight, onResult, onError }: Props) {
  const [body, setBody] = useState('')
  const [width, setWidth] = useState(String(screenWidth))
  const [height, setHeight] = useState(String(screenHeight))
  const [align, setAlign] = useState<Align>('center')
  const [share, setShare] = useState(1)
  const [reflow, setReflow] = useState<Reflow>('auto')

  // 呼び出し側が毎回作り直す関数を渡してきても、描き直しの合図が
  // 増えないようにする。ここが依存に入ると、描く→親が状態を変える→
  // また描く、で止まらなくなる。
  const callbacks = useRef({ onResult, onError })
  callbacks.current = { onResult, onError }

  const webview = useRef<WebView>(null)
  // 読み込みが終わるまで描けない。終わる前の依頼は覚えておいて、あとで出す。
  const ready = useRef(false)
  const waiting = useRef<DrawRequest | null>(null)
  // 打っている間に何度も返ってくるので、最後に頼んだものだけを受け取る。
  const latest = useRef(0)

  const draw = useCallback(() => {
    latest.current += 1
    const request: DrawRequest = {
      id: latest.current,
      body,
      width: size(width, screenWidth),
      height: size(height, screenHeight),
      share,
      align,
      reflow,
    }

    if (request.body.trim() === '') {
      callbacks.current.onResult({ kind: 'empty' })
      return
    }

    if (!ready.current) {
      waiting.current = request
      return
    }
    webview.current?.injectJavaScript(`window.draw(${JSON.stringify(request)}); true;`)
  }, [align, body, height, reflow, screenHeight, screenWidth, share, width])

  // 1 文字ごとに描き直すと、そのたびに二分探索と PNG の生成が走る。
  // 手が止まってからにする。
  useEffect(() => {
    const timer = setTimeout(draw, 300)
    return () => clearTimeout(timer)
  }, [draw])

  function receive(raw: string) {
    let result: DrawResult
    try {
      result = JSON.parse(raw) as DrawResult
    } catch {
      callbacks.current.onError('文字を画像にできませんでした')
      return
    }

    if (result.ready) {
      ready.current = true
      if (waiting.current) {
        const request = waiting.current
        waiting.current = null
        webview.current?.injectJavaScript(`window.draw(${JSON.stringify(request)}); true;`)
      }
      return
    }

    // 追い抜かれた結果は捨てる。古い絵を送ってしまわないため。
    if (result.id !== latest.current) {
      return
    }

    if (result.error || !result.dataUrl) {
      callbacks.current.onError(result.error ?? '文字を画像にできませんでした')
      return
    }

    // 収まらなかったときは白いままの画像が返る。送らせない。
    if (!result.size) {
      callbacks.current.onResult({ kind: 'overflow' })
      return
    }

    try {
      callbacks.current.onResult({
        kind: 'ready',
        size: result.size,
        image: {
          bytes: bytesFromDataUrl(result.dataUrl),
          uri: result.dataUrl,
          width: result.width ?? 0,
          height: result.height ?? 0,
        },
      })
    } catch (e) {
      callbacks.current.onError(e instanceof Error ? e.message : '文字を画像にできませんでした')
    }
  }

  return (
    <View>
      <TextInput
        style={styles.input}
        value={body}
        onChangeText={setBody}
        placeholder={'表示する文字\n改行するとそのまま改行されます'}
        placeholderTextColor="#999"
        multiline
        textAlignVertical="top"
        inputAccessoryViewID={accessoryFor}
      />

      <View style={styles.row}>
        <View style={styles.field}>
          <Text style={styles.caption}>幅</Text>
          <TextInput
            style={styles.number}
            value={width}
            onChangeText={setWidth}
            keyboardType="number-pad"
            inputAccessoryViewID={accessoryFor}
          />
        </View>
        <View style={styles.field}>
          <Text style={styles.caption}>高さ</Text>
          <TextInput
            style={styles.number}
            value={height}
            onChangeText={setHeight}
            keyboardType="number-pad"
            inputAccessoryViewID={accessoryFor}
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
        <Text style={styles.caption}>折り返し</Text>
        {REFLOWS.map((option) => (
          <Pressable
            key={option.value}
            style={[styles.chip, reflow === option.value && styles.chipOn]}
            onPress={() => setReflow(option.value)}>
            <Text style={[styles.chipLabel, reflow === option.value && styles.chipLabelOn]}>
              {option.label}
            </Text>
          </Pressable>
        ))}
      </View>

      <View style={styles.row}>
        <Text style={styles.caption}>文字寄せ</Text>
        {ALIGNS.map((option) => (
          <Pressable
            key={option.value}
            style={[styles.chip, align === option.value && styles.chipOn]}
            onPress={() => setAlign(option.value)}>
            <Text style={[styles.chipLabel, align === option.value && styles.chipLabelOn]}>
              {option.label}
            </Text>
          </Pressable>
        ))}
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
        本体の画面は {screenWidth} x {screenHeight} です。同じ大きさにすると、
        拡大されずいちばんきれいに出ます。
      </Text>

      {Platform.OS === 'ios' && (
        <InputAccessoryView nativeID={ACCESSORY_ID}>
          <View style={styles.accessory}>
            <Pressable style={styles.close} onPress={() => Keyboard.dismiss()}>
              <Text style={styles.closeLabel}>閉じる</Text>
            </Pressable>
          </View>
        </InputAccessoryView>
      )}

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
  input: {
    minHeight: 96,
    padding: 12,
    fontSize: 16,
    lineHeight: 24,
    color: '#222',
    borderWidth: 1,
    borderColor: '#bbb',
    borderRadius: 8,
  },
  row: { flexDirection: 'row', alignItems: 'center', gap: 8, marginTop: 12 },
  field: { flex: 1, flexDirection: 'row', alignItems: 'center', gap: 6 },
  caption: { fontSize: 13, color: '#555' },
  number: {
    flex: 1,
    paddingVertical: 8,
    paddingHorizontal: 10,
    fontSize: 15,
    color: '#222',
    borderWidth: 1,
    borderColor: '#bbb',
    borderRadius: 6,
  },
  chip: {
    paddingVertical: 8,
    paddingHorizontal: 12,
    borderWidth: 1,
    borderColor: '#1257a0',
    borderRadius: 6,
  },
  chipOn: { backgroundColor: '#1257a0' },
  chipLabel: { fontSize: 13, fontWeight: '600', color: '#1257a0' },
  chipLabelOn: { color: '#fff' },
  hint: { marginTop: 10, fontSize: 12, color: '#777', lineHeight: 18 },
  accessory: {
    alignItems: 'flex-end',
    paddingHorizontal: 12,
    paddingVertical: 6,
    backgroundColor: '#f4f4f5',
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: '#c8c8cc',
  },
  close: { paddingVertical: 6, paddingHorizontal: 12 },
  closeLabel: { fontSize: 16, fontWeight: '600', color: '#1257a0' },
  // 絵を作るためだけの WebView。見せる必要はないが、
  // 大きさを 0 にすると端末によっては動かないので 1px 残す。
  hidden: { position: 'absolute', width: 1, height: 1, opacity: 0 },
})
