import test from 'node:test'
import assert from 'node:assert/strict'

import { TEXT_RENDER_HTML } from '../src/textRender.ts'

/**
 * 名刺の組み方と QR の作り方の試験。
 *
 * textRender.test.ts と同じく、目印で挟んだところを取り出して、
 * 文字の幅を返すだけの偽の canvas で動かす。ここで見ているのは本体
 * （lib/WebTransfer）と同じコードでもある。
 */
function load() {
  const begin = TEXT_RENDER_HTML.indexOf('>>> shared-text-render')
  const end = TEXT_RENDER_HTML.indexOf('<<< shared-text-render')
  const code = TEXT_RENDER_HTML.slice(begin + '>>> shared-text-render'.length, end)
  // 最後の行は目印のコメントの途中で切れている。改行を挟まないと続きも消える。
  const make = new Function(`${code}\n; return { paintCard, qrModules, qrFits };`)
  return make() as {
    paintCard: PaintCard
    qrModules: (text: string) => number[][] | null
    qrFits: (text: string) => boolean
  }
}

type Card = {
  image?: { element: string; width: number; height: number } | null
  title?: string
  subtitle?: string
  account?: string
  url?: string
}

type PaintCard = (
  ctx: unknown,
  width: number,
  height: number,
  card: Card,
) => { drawn: boolean; size: number }

type Drawn = { text: string; x: number; y: number; size: number }
type Placed = { x: number; y: number; width: number; height: number }

/**
 * 偽の canvas。textRender.test.ts のものに、画像と塗りつぶしの記録を足してある。
 * QR は塗りつぶしで描かれるので、升目の大きさはそこから分かる。
 */
function fakeContext() {
  let size = 10
  const drawn: Drawn[] = []
  const images: Placed[] = []
  const rects: Placed[] = []
  return {
    drawn,
    images,
    rects,
    set font(value: string) {
      size = Number(/(\d+)px/.exec(value)?.[1] ?? 10)
    },
    fillStyle: '',
    textAlign: '',
    textBaseline: '',
    fillRect(x: number, y: number, width: number, height: number) {
      rects.push({ x, y, width, height })
    },
    drawImage(element: string, x: number, y: number, width: number, height: number) {
      images.push({ x, y, width, height })
    },
    fillText(text: string, x: number, y: number) {
      drawn.push({ text, x, y, size })
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

/** 名刺に載せる画像。中身は使わないので、大きさだけあればよい。 */
const IMAGE = { element: 'icon', width: 400, height: 400 }

const URL = 'https://x.com/mitsuharu_e'

test('項目が何も無ければ描かない', () => {
  const { paintCard } = load()
  const ctx = fakeContext()
  assert.deepEqual(paintCard(ctx, 540, 960, {}), { drawn: false, size: 0 })
  assert.equal(ctx.drawn.length, 0)
  assert.equal(ctx.images.length, 0)
})

test('タイトルとサブタイトルを並べる', () => {
  const { paintCard } = load()
  const ctx = fakeContext()
  const result = paintCard(ctx, 540, 960, { title: '江本 光晴', subtitle: 'Mitsuharu Emoto' })
  assert.ok(result.drawn)
  assert.deepEqual(ctx.drawn.map((line) => line.text), ['江本 光晴', 'Mitsuharu Emoto'])
})

test('サブタイトルとアカウントはタイトルより小さい', () => {
  // 名刺の見た目として、英字表記やアカウントは名前より小さくする。
  const { paintCard } = load()
  const ctx = fakeContext()
  paintCard(ctx, 540, 960, {
    title: '江本 光晴',
    subtitle: 'Mitsuharu Emoto',
    account: '@mitsuharu_e',
  })
  const [title, subtitle, account] = ctx.drawn
  assert.ok(title.size > subtitle.size, `${title.size} > ${subtitle.size}`)
  assert.ok(subtitle.size > account.size, `${subtitle.size} > ${account.size}`)
})

test('空の項目は場所を取らず、残りが真ん中に来る', () => {
  // 入力する項目は決まっているが、全部埋める必要は無い。
  // 埋めなかったぶんが空いたままだと、名刺として見たときに間が抜ける。
  const { paintCard } = load()
  const ctx = fakeContext()
  paintCard(ctx, 540, 960, { title: '江本 光晴' })
  const [line] = ctx.drawn
  assert.ok(Math.abs(line.y - 480) < 2, `${line.y}`)
  assert.equal(line.x, 270)
})

test('縦長のときは、画像・文字・QR を縦に積む', () => {
  const { paintCard } = load()
  const ctx = fakeContext()
  const result = paintCard(ctx, 540, 960, {
    image: IMAGE,
    title: '江本 光晴',
    subtitle: 'Mitsuharu Emoto',
    url: URL,
  })
  assert.ok(result.drawn)

  const image = ctx.images[0]
  const text = ctx.drawn[0]
  // QR は最後に置かれるので、いちばん下の塗りつぶしがその位置になる
  const qr = ctx.rects[ctx.rects.length - 1]
  assert.ok(image.y + image.height <= text.y, '画像が文字より上にない')
  assert.ok(text.y < qr.y, '文字が QR より上にない')
})

test('横長で画像があるときは、画像を左に置いて右に積む', () => {
  // 1 列に積むと画像が潰れ、左右が空いたままになる。
  const { paintCard } = load()
  const ctx = fakeContext()
  const result = paintCard(ctx, 960, 540, {
    image: IMAGE,
    title: '江本 光晴',
    subtitle: 'Mitsuharu Emoto',
    url: URL,
  })
  assert.ok(result.drawn)

  const image = ctx.images[0]
  assert.ok(image.x + image.width <= 480, `画像が左半分に収まっていない: ${image.x + image.width}`)
  for (const line of ctx.drawn) {
    assert.ok(line.x > 480, `文字が右側にない: ${line.x}`)
  }
})

test('横長でも画像が無ければ 1 列に積む', () => {
  const { paintCard } = load()
  const ctx = fakeContext()
  paintCard(ctx, 960, 540, { title: '江本 光晴', url: URL })
  assert.equal(ctx.drawn[0].x, 480)
})

test('画像だけでも名刺になる', () => {
  const { paintCard } = load()
  const ctx = fakeContext()
  const result = paintCard(ctx, 540, 960, { image: IMAGE })
  // 文字が無いので大きさは 0 になるが、描けている
  assert.deepEqual(result, { drawn: true, size: 0 })
  assert.equal(ctx.images.length, 1)
})

test('画像は縦横の比を変えずに収める', () => {
  const { paintCard } = load()
  const ctx = fakeContext()
  paintCard(ctx, 540, 960, { image: { element: 'icon', width: 400, height: 200 } })
  const image = ctx.images[0]
  assert.equal(image.width, image.height * 2)
})

test('QR の升目は整数の大きさで描く', () => {
  // 電子ペーパーは階調が粗く、半端な大きさで描くと升目の境が濁って読めない。
  const { paintCard } = load()
  const ctx = fakeContext()
  paintCard(ctx, 540, 960, { url: URL })
  // 最初の 1 つは名刺の下地、次が QR の下地。そのあとが升目。
  const modules = ctx.rects.slice(2)
  assert.ok(modules.length > 100)
  for (const rect of modules) {
    assert.equal(rect.width, modules[0].width)
    assert.equal(rect.height, modules[0].width)
    assert.equal(rect.width, Math.round(rect.width))
  }
})

test('QR にする URL が長すぎるときは qrFits が知らせる', () => {
  // 描く前に呼び出し側が弾く。8 ビットモードの型番 10 までで 213 バイト。
  const { qrFits } = load()
  assert.ok(qrFits('x'.repeat(213)))
  assert.ok(!qrFits('x'.repeat(214)))
  // 日本語は 1 文字 3 バイト
  assert.ok(qrFits('あ'.repeat(71)))
  assert.ok(!qrFits('あ'.repeat(72)))
})

test('決まった URL からは決まった升目ができる', () => {
  // 読み取り機で復号できることを確かめた並びを、そのまま置いてある。
  // 誤り訂正やマスクの選び方を変えると、ここが合わなくなる。
  const { qrModules } = load()
  const expected = [
    '#######....##.#.#.#######',
    '#.....#...#.##.#..#.....#',
    '#.###.#.####..###.#.###.#',
    '#.###.#.##.######.#.###.#',
    '#.###.#.##....#.#.#.###.#',
    '#.....#.###...###.#.....#',
    '#######.#.#.#.#.#.#######',
    '........#.##..##.........',
    '#.#####....#.##...#####..',
    '...##..####.##..#......#.',
    '#.###.##..##...###...#.##',
    '...#.#..#...#..##...#...#',
    '#.#...#.###.###..####.###',
    '#.#.....##......#..#.#.#.',
    '#.....#...###..##.####.##',
    '#.###..#...#..#.##.##...#',
    '#.....#.#..####.#####.#..',
    '........#...#.###...##...',
    '#######..##..#..#.#.#.###',
    '#.....#.###....##...##.##',
    '#.###.#.#.#.#########.#..',
    '#.###.#.####..#..##.#####',
    '#.###.#.##....#.#....##.#',
    '#.....#...##..####.###..#',
    '#######.####..#..#.######',
  ]
  const modules = qrModules(URL)
  assert.ok(modules !== null)
  assert.deepEqual(modules.map((row) => row.map((on) => (on ? '#' : '.')).join('')), expected)
})

test('文字数に応じて QR の型番が上がる', () => {
  const { qrModules } = load()
  // 型番の大きさは 4n + 17
  assert.equal(qrModules('a')?.length, 21)
  assert.equal(qrModules('a'.repeat(30))?.length, 29)
  assert.equal(qrModules('a'.repeat(213))?.length, 57)
  assert.equal(qrModules('a'.repeat(214)), null)
})

test('QR の四隅にはファインダが立つ', () => {
  // ここが崩れると、読み取り機が符号を見つけられない。
  const { qrModules } = load()
  const modules = qrModules(URL)
  assert.ok(modules !== null)
  const size = modules.length
  for (const [top, left] of [[0, 0], [0, size - 7], [size - 7, 0]]) {
    for (let y = 0; y < 7; y++) {
      for (let x = 0; x < 7; x++) {
        const ring = x === 0 || x === 6 || y === 0 || y === 6
        const core = x >= 2 && x <= 4 && y >= 2 && y <= 4
        assert.equal(modules[top + y][left + x], ring || core ? 1 : 0, `(${top + y}, ${left + x})`)
      }
    }
  }
})

test('URL が空なら QR を置かない', () => {
  const { paintCard } = load()
  const ctx = fakeContext()
  paintCard(ctx, 540, 960, { title: '江本 光晴', url: '   ' })
  // 名刺の下地を塗るだけで、升目は描かれない
  assert.equal(ctx.rects.length, 1)
})
