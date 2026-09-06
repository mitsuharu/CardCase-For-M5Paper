/**
 * base64 をバイト列に戻す。
 *
 * WebView から画像を受け取る経路は文字列しか通らないので、
 * 返ってくる data URL を自分で開く必要がある。
 *
 * React Native の atob は端末やエンジンの版で有無が変わる。
 * ここは数十行で書けるうえ、実機の API を使わない計算なので、
 * 自分で持って node から試験する。
 */

const TABLE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'

/** 逆引き。1 文字ずつ indexOf すると入力の長さの 2 乗になる。 */
const INDEX = (() => {
  const table = new Int16Array(128).fill(-1)
  for (let i = 0; i < TABLE.length; i++) {
    table[TABLE.charCodeAt(i)] = i
  }
  return table
})()

/**
 * data URL から中身を取り出す。
 * base64 でないものは扱えないので、そのときは投げる。
 */
export function bytesFromDataUrl(dataUrl: string): Uint8Array {
  const marker = ';base64,'
  const at = dataUrl.indexOf(marker)
  if (!dataUrl.startsWith('data:') || at < 0) {
    throw new Error('画像を取り出せませんでした')
  }
  return decodeBase64(dataUrl.slice(at + marker.length))
}

export function decodeBase64(input: string): Uint8Array {
  // 詰め物と改行を落とす。data URL に改行は入らないが、
  // 手で貼ったものを渡されても困らないようにしておく。
  let text = ''
  for (const character of input) {
    if (character === '=' || character === '\n' || character === '\r') {
      continue
    }
    const code = character.charCodeAt(0)
    if (code > 127 || INDEX[code] < 0) {
      throw new Error('画像を取り出せませんでした')
    }
    text += character
  }

  // 4 文字が 3 バイトになる。余りは切り捨てず、書けるところまで書く。
  const bytes = new Uint8Array(Math.floor((text.length * 3) / 4))
  let at = 0
  let buffer = 0
  let bits = 0
  for (let i = 0; i < text.length; i++) {
    buffer = (buffer << 6) | INDEX[text.charCodeAt(i)]
    bits += 6
    if (bits >= 8) {
      bits -= 8
      bytes[at++] = (buffer >> bits) & 0xff
    }
  }
  return bytes
}
