import assert from 'node:assert/strict'
import { test } from 'node:test'

import {
  Command,
  MAGIC_0,
  MAGIC_1,
  RESPONSE_HEADER_SIZE,
  Status,
  VERSION,
  crc32,
  frame,
  isValidResponse,
  readU16,
  readU32,
  responseStatus,
  statusName,
  writeU32,
} from '../src/protocol.ts'

test('CRC-32 が既知の値と一致する', () => {
  // 本体側の test_crc32_matches_known_values と同じ値。
  // ここが食い違うと、送った画像が必ず壊れていると判定される。
  assert.equal(crc32(new TextEncoder().encode('123456789')), 0xcbf43926)
  assert.equal(crc32(new Uint8Array(0)), 0x00000000)
  assert.equal(crc32(new Uint8Array([0x00])), 0xd202ef8d)
})

test('CRC-32 は符号なしで返る', () => {
  // 32bit の最上位が立つ値。>>> 0 を忘れると負の数になり、
  // 本体に渡す 4 バイトが変わってしまう。
  const value = crc32(new Uint8Array([0xff, 0xff, 0xff, 0xff]))
  assert.ok(value >= 0, `符号なしのはずが ${value}`)
  assert.ok(value <= 0xffffffff)
})

test('フレームの先頭は印と版とコマンド', () => {
  assert.deepEqual(frame(Command.Hello), [MAGIC_0, MAGIC_1, VERSION, Command.Hello])
  assert.deepEqual(frame(Command.Data, [1, 2]), [MAGIC_0, MAGIC_1, VERSION, Command.Data, 1, 2])
})

test('自分あての応答だけを受け入れる', () => {
  const ok = [MAGIC_0, MAGIC_1, VERSION, Command.Hello, Status.Ok]
  assert.equal(isValidResponse(ok, Command.Hello), true)

  assert.equal(isValidResponse(ok, Command.Data), false, 'コマンドが違う')
  assert.equal(isValidResponse([0x00, MAGIC_1, VERSION, Command.Hello, 0], Command.Hello), false, '印が違う')
  assert.equal(isValidResponse([MAGIC_0, MAGIC_1, 0x02, Command.Hello, 0], Command.Hello), false, '版が違う')
  assert.equal(isValidResponse(ok.slice(0, RESPONSE_HEADER_SIZE - 1), Command.Hello), false, '短すぎる')
  assert.equal(isValidResponse([], Command.Hello), false, '空')
})

test('状態は 5 バイト目から読む', () => {
  assert.equal(responseStatus([MAGIC_0, MAGIC_1, VERSION, Command.Hello, Status.NotReady]), Status.NotReady)
})

test('32bit は大きい方が先', () => {
  assert.deepEqual(writeU32(0x01020304), [0x01, 0x02, 0x03, 0x04])
  assert.deepEqual(writeU32(0), [0, 0, 0, 0])
  assert.deepEqual(writeU32(0xffffffff), [0xff, 0xff, 0xff, 0xff])

  assert.equal(readU32([0x01, 0x02, 0x03, 0x04], 0), 0x01020304)
  assert.equal(readU16([0x12, 0x34], 0), 0x1234)
})

test('最上位が立った 32bit を読んでも負にならない', () => {
  // 画像の大きさや CRC でここに入る。符号付きで扱うと桁が化ける。
  const value = readU32(writeU32(0xdeadbeef), 0)
  assert.equal(value, 0xdeadbeef)
  assert.ok(value > 0)
})

test('状態の名前が本体の一覧と揃っている', () => {
  assert.equal(statusName(Status.Ok), 'OK')
  assert.equal(statusName(Status.NotReady), 'NOT_READY')
  assert.equal(statusName(Status.CrcMismatch), 'CRC_MISMATCH')
  assert.match(statusName(0x7f), /UNKNOWN/)
})
