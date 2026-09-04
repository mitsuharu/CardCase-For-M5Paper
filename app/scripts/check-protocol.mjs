// 本体とアプリで、やり取りの決まりが食い違っていないか確かめる。
//
//   node scripts/check-protocol.mjs
//
// src/protocol.ts は lib/NfcTransfer/Protocol/Protocol.h と対になっている。
// 片方だけ直すと、その場では気づかず、実機でつないだときに初めて転送が
// 失敗する。人が見比べるには数が多いので機械に見せる。

import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..')
const header = readFileSync(join(root, 'lib/NfcTransfer/Protocol/Protocol.h'), 'utf8')
const source = readFileSync(join(root, 'app/src/protocol.ts'), 'utf8')

// 対応表。左が本体、右がアプリ。
const scalars = [
  ['kMagic0', 'MAGIC_0'],
  ['kMagic1', 'MAGIC_1'],
  ['kVersion', 'VERSION'],
  ['kMaxFrameSize', 'MAX_FRAME_SIZE'],
  ['kHeaderSize', 'HEADER_SIZE'],
  ['kResponseHeaderSize', 'RESPONSE_HEADER_SIZE'],
  ['kMaxChunkSize', 'MAX_CHUNK_SIZE'],
]
const commands = ['Hello', 'Begin', 'Data', 'Commit', 'Abort']
const statuses = [
  'Ok', 'Accepted', 'BadRequest', 'BadOffset',
  'TooLarge', 'NoTransfer', 'CrcMismatch', 'Incomplete', 'NotReady',
]

// 式で書かれている値があるので、数に直してから比べる。
// 例: kMaxFrameSize - (kHeaderSize + 4 + 4)
function evaluate(expression, known) {
  const substituted = expression
    .replace(/'(.)'/g, (_, c) => String(c.charCodeAt(0)))     // 'C' → 67
    .replace(/0x[0-9a-f]+/gi, (hex) => String(Number(hex)))   // 0x43 → 67
    .replace(/\b(\d+)u\b/gi, '$1')                             // 256u → 256
    .replace(/(?<![\w.])[A-Za-z_]\w*/g, (name) => {
      if (!(name in known)) throw new Error(`知らない名前: ${name}`)
      return String(known[name])
    })
  if (!/^[\d\s+\-*/()]+$/.test(substituted)) {
    throw new Error(`式として読めない: ${expression}`)
  }
  return Function(`return (${substituted})`)()
}

function readHeader() {
  const values = {}
  const re = /const\s+(?:uint8_t|uint32_t|size_t)\s+(\w+)\s*=\s*([^;]+);/g
  for (const [, name, expression] of header.matchAll(re)) {
    values[name] = evaluate(expression.trim(), values)
  }
  for (const block of ['Command', 'Status']) {
    const body = header.match(new RegExp(`enum ${block}\\s*:[^{]*\\{([^}]*)\\}`))
    if (!body) throw new Error(`${block} が見つからない`)
    for (const [, name, value] of body[1].matchAll(/(\w+)\s*=\s*(0x[0-9a-f]+|\d+)/gi)) {
      values[name] = Number(value)
    }
  }
  return values
}

function readSource() {
  const values = {}
  for (const [, name, expression] of source.matchAll(/export const (\w+) = ([^;\n]+)/g)) {
    if (expression.trim().startsWith('{')) continue
    values[name] = evaluate(expression.trim(), values)
  }
  for (const block of ['Command', 'Status']) {
    const body = source.match(new RegExp(`export const ${block} = \\{([^}]*)\\}`))
    if (!body) throw new Error(`${block} が見つからない`)
    for (const [, name, value] of body[1].matchAll(/(\w+):\s*(0x[0-9a-f]+|\d+)/gi)) {
      values[`${block}.${name}`] = Number(value)
    }
  }
  return values
}

const device = readHeader()
const app = readSource()
const problems = []

function compare(deviceName, appName) {
  const a = device[deviceName]
  const b = app[appName]
  if (a === undefined) problems.push(`本体に ${deviceName} が無い`)
  else if (b === undefined) problems.push(`アプリに ${appName} が無い`)
  else if (a !== b) problems.push(`${deviceName}=${a} と ${appName}=${b} が食い違う`)
}

for (const [deviceName, appName] of scalars) compare(deviceName, appName)
for (const name of commands) compare(`Command${name}`, `Command.${name}`)
for (const name of statuses) compare(`Status${name}`, `Status.${name}`)

if (problems.length > 0) {
  console.error('本体とアプリで決まりが食い違っている:\n')
  for (const problem of problems) console.error(`  - ${problem}`)
  console.error('\nlib/NfcTransfer/Protocol/Protocol.h と app/src/protocol.ts の両方を直すこと。')
  console.error('docs/nfc-protocol.md も忘れずに。')
  process.exit(1)
}

const checked = scalars.length + commands.length + statuses.length
console.log(`本体とアプリで ${checked} 個の値が一致している`)
