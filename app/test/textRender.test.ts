import test from 'node:test'
import assert from 'node:assert/strict'

import { TEXT_RENDER_HTML } from '../src/textRender.ts'

/**
 * 描画のコードは WebView（ブラウザ）で動くものなので、node からは
 * canvas を用意できない。文字の幅を返すだけの偽物を渡して、
 * 折り返しと大きさの決め方だけを試験する。
 *
 * ここで見ているのは本体（lib/WebTransfer）と同じコードでもある。
 * 食い違っていないことは scripts/check-text-render.mjs が見る。
 */
function load() {
  const begin = TEXT_RENDER_HTML.indexOf('>>> shared-text-render')
  const end = TEXT_RENDER_HTML.indexOf('<<< shared-text-render')
  const code = TEXT_RENDER_HTML.slice(begin + '>>> shared-text-render'.length, end)
  // 最後の行は目印のコメントの途中で切れている。改行を挟まないと続きも消える。
  const make = new Function(`${code}\n; return { paintText };`)
  return make() as { paintText: PaintText }
}

type PaintText = (
  ctx: unknown,
  width: number,
  height: number,
  body: string,
  share: number,
  align: 'left' | 'center' | 'right',
  reflow: 'auto' | 'keep',
) => number

type Drawn = { text: string; x: number; y: number }

/**
 * 偽の canvas。
 * 全角は 1 文字ぶん、半角はその半分強の幅にしてある。実際のフォントとは
 * 違うが、折り返しの判断は幅の大小しか見ていないので、これで足りる。
 */
function fakeContext() {
  let size = 10
  const drawn: Drawn[] = []
  return {
    drawn,
    get size() {
      return size
    },
    set font(value: string) {
      size = Number(/(\d+)px/.exec(value)?.[1] ?? 10)
    },
    fillStyle: '',
    textAlign: '',
    textBaseline: '',
    fillRect() {},
    fillText(text: string, x: number, y: number) {
      drawn.push({ text, x, y })
    },
    measureText(text: string) {
      let width = 0
      for (const character of text) {
        width += character.charCodeAt(0) < 128 ? size * 0.55 : size
      }
      return { width }
    },
  }
}

test('空の文字は描かない', () => {
  const { paintText } = load()
  const ctx = fakeContext()
  assert.equal(paintText(ctx, 480, 800, '   \n  ', 1, 'center', 'auto'), 0)
  assert.equal(ctx.drawn.length, 0)
})

test('改行はそのまま行になる', () => {
  const { paintText } = load()
  const ctx = fakeContext()
  paintText(ctx, 480, 800, '山田 太郎\nCardCase', 1, 'center', 'auto')
  assert.deepEqual(ctx.drawn.map((line) => line.text), ['山田 太郎', 'CardCase'])
})

test('語の途中では折り返さない', () => {
  const { paintText } = load()
  const ctx = fakeContext()
  paintText(ctx, 480, 800, '山田 太郎\nyamada@example.com', 1, 'center', 'auto')
  assert.ok(
    ctx.drawn.some((line) => line.text === 'yamada@example.com'),
    `割れている: ${JSON.stringify(ctx.drawn.map((line) => line.text))}`,
  )
})

test('枠に入らないほど長い語は文字単位で割る', () => {
  const { paintText } = load()
  const ctx = fakeContext()
  // どんなに小さくしても 1 行には入らない長さ。割らなければ描けない。
  paintText(ctx, 300, 300, 'A'.repeat(200), 1, 'center', 'auto')
  assert.ok(ctx.drawn.length > 1)
  assert.equal(ctx.drawn.map((line) => line.text).join(''), 'A'.repeat(200))
})

test('どうやっても入らないときは何も描かない', () => {
  // 呼び出し側はこれを見て「入らない」と知らせ、送信を止める。
  // 白いままの画像を送ってしまうより分かりやすい。
  const { paintText } = load()
  const ctx = fakeContext()
  assert.equal(paintText(ctx, 120, 60, 'A'.repeat(400), 1, 'center', 'auto'), 0)
  assert.equal(ctx.drawn.length, 0)
})

test('書いたとおりの行を優先する', () => {
  // 日本語は語の切れ目が無いので、折り返しを先に許すと「山田 太」「郎」で
  // 割ってでも字を大きくしてしまう。改行は書いた人の指定として扱う。
  const { paintText } = load()
  const ctx = fakeContext()
  paintText(ctx, 480, 800, '山田 太郎\n株式会社みつわ', 1, 'center', 'auto')
  assert.deepEqual(ctx.drawn.map((line) => line.text), ['山田 太郎', '株式会社みつわ'])
})

test('長い 1 行は、読めなくなるくらいなら折り返す', () => {
  // 書いた改行を優先しすぎると、1 行に押し込もうとして極端に小さくなる。
  // 名刺として使えるかどうかはそちらで決まるので、折り返しに任せる。
  const { paintText } = load()
  const ctx = fakeContext()
  const size = paintText(ctx, 480, 800, 'あ'.repeat(60), 1, 'center', 'auto')
  assert.ok(ctx.drawn.length > 1, '折り返していない')
  assert.ok(size >= 12, `${size}px は小さすぎる`)
})

test('折り返さないを選ぶと、小さくなっても書いた行のままにする', () => {
  // 折り返されると意味が変わるもの（表の見出しなど）のための逃げ道。
  // 読めない大きさになっても、選んだとおりにする。
  const { paintText } = load()
  const ctx = fakeContext()
  const size = paintText(ctx, 480, 800, 'あ'.repeat(60), 1, 'center', 'keep')
  assert.equal(ctx.drawn.length, 1)
  assert.ok(size < 12, `${size}px（折り返さないので小さくなるはず）`)
})

test('割合を下げると文字が小さくなる', () => {
  const { paintText } = load()
  const auto = paintText(fakeContext(), 480, 800, '山田 太郎\nCardCase', 1, 'center', 'auto')
  const half = paintText(fakeContext(), 480, 800, '山田 太郎\nCardCase', 0.5, 'center', 'auto')
  assert.ok(auto > 0)
  assert.ok(half < auto, `${half} < ${auto}`)
  assert.equal(half, Math.round(auto * 0.5))
})

test('割合を下げても 12px より小さくはしない', () => {
  const { paintText } = load()
  const auto = paintText(fakeContext(), 480, 800, '山田 太郎', 1, 'center', 'auto')
  assert.ok(auto > 12)
  assert.equal(paintText(fakeContext(), 480, 800, '山田 太郎', 0.05, 'center', 'auto'), 12)
})

test('文字数が多くて自動でも 12px を切る場合は、収めるほうを優先する', () => {
  const { paintText } = load()
  const size = paintText(fakeContext(), 200, 200, 'あ'.repeat(600), 1, 'center', 'auto')
  assert.ok(size > 0)
  assert.ok(size < 12, `${size}`)
})

test('寄せで描く位置が変わる', () => {
  const { paintText } = load()
  const padding = Math.round(Math.min(480, 800) * 0.08)

  const left = fakeContext()
  paintText(left, 480, 800, 'CardCase', 1, 'left', 'auto')
  assert.ok(left.drawn.every((line) => line.x === padding))

  const center = fakeContext()
  paintText(center, 480, 800, 'CardCase', 1, 'center', 'auto')
  assert.ok(center.drawn.every((line) => line.x === 240))

  const right = fakeContext()
  paintText(right, 480, 800, 'CardCase', 1, 'right', 'auto')
  assert.ok(right.drawn.every((line) => line.x === 480 - padding))
})

test('縦は中央に置く', () => {
  const { paintText } = load()
  const ctx = fakeContext()
  paintText(ctx, 480, 800, '1\n2\n3', 1, 'center', 'auto')
  const middle = (ctx.drawn[0].y + ctx.drawn[ctx.drawn.length - 1].y) / 2
  assert.ok(Math.abs(middle - 400) < 1, `${middle}`)
})

test('枠に収まる', () => {
  const { paintText } = load()
  const ctx = fakeContext()
  const size = paintText(ctx, 480, 800, '山田 太郎\nCardCase\nyamada@example.com', 1, 'center', 'auto')
  const padding = Math.round(480 * 0.08)
  const lineHeight = Math.ceil(size * 1.35)
  assert.ok(ctx.drawn.length * lineHeight <= 800 - padding * 2)
})
