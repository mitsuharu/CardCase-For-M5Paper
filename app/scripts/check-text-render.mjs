// 本体とアプリで、文字の描き方が食い違っていないか確かめる。
//
//   node scripts/check-text-render.mjs
//
// app/src/textRender.ts の描画コードは、本体の配信ページ
// lib/WebTransfer/WebTransfer.cpp に埋め込まれたものと対になっている。
// 片方だけ直すと、WiFi で送った名刺と NFC で送った名刺で字の大きさや
// 折り返しが変わる。並べて比べないと気づけないので機械に見せる。

import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..')

const BEGIN = '>>> shared-text-render'
const END = '<<< shared-text-render'

/// 目印で挟んだところを取り出す
function extract(path) {
  const source = readFileSync(join(root, path), 'utf8')
  const begin = source.indexOf(BEGIN)
  const end = source.indexOf(END)
  if (begin < 0 || end < 0 || end < begin) {
    console.error(`${path}: 目印（${BEGIN} / ${END}）が見つからない`)
    process.exit(1)
  }
  return source.slice(begin + BEGIN.length, end)
}

const firmware = extract('lib/WebTransfer/WebTransfer.cpp')
const app = extract('app/src/textRender.ts')

if (firmware === app) {
  const lines = firmware.trim().split('\n').length
  console.log(`文字の描き方は本体とアプリで一致している（${lines} 行）`)
  process.exit(0)
}

// どこが違うのかまで出す。行数が多いので、食い違う行だけを並べる。
const left = firmware.split('\n')
const right = app.split('\n')
console.error('文字の描き方が本体とアプリで食い違っている\n')
for (let i = 0; i < Math.max(left.length, right.length); i++) {
  if (left[i] !== right[i]) {
    console.error(`  ${i + 1} 行目`)
    console.error(`    lib/WebTransfer/WebTransfer.cpp: ${left[i] ?? '(無し)'}`)
    console.error(`    app/src/textRender.ts:           ${right[i] ?? '(無し)'}`)
  }
}
console.error('\nどちらかに合わせて直すこと。')
process.exit(1)
