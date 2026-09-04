import assert from 'node:assert/strict'
import { test } from 'node:test'

import {
  MAX_STALLED,
  chunkLimitFromTransceive,
  effectiveChunkSize,
  isFatal,
  nextStalledCount,
  shouldGiveUp,
} from '../src/chunking.ts'
import { NfcCancelled, NfcError } from '../src/errors.ts'
import { MAX_CHUNK_SIZE } from '../src/protocol.ts'

test('端末の上限から見出しの分を引く', () => {
  // 送るのは見出し 4 + 転送の番号 4 + 位置 4 を含めた全体
  assert.equal(chunkLimitFromTransceive(128), 116)
  assert.equal(chunkLimitFromTransceive(64), 52)
})

test('端末が大きくても決まりの上限を超えない', () => {
  // 253 バイトは ISO14443-A の限界。端末が扱えても超えられない。
  assert.equal(chunkLimitFromTransceive(4096), MAX_CHUNK_SIZE)
})

test('上限を取れなかったら決まりの値を使う', () => {
  // getMaxTransceiveLength は端末によって 0 や null を返す
  assert.equal(chunkLimitFromTransceive(0), MAX_CHUNK_SIZE)
  assert.equal(chunkLimitFromTransceive(null), MAX_CHUNK_SIZE)
  assert.equal(chunkLimitFromTransceive(undefined), MAX_CHUNK_SIZE)
  assert.equal(chunkLimitFromTransceive(-1), MAX_CHUNK_SIZE)
})

test('見出しより短い上限でも 1 以上になる', () => {
  // 0 を返すと送る中身が無くなり、進まないまま繰り返す
  assert.equal(chunkLimitFromTransceive(4), 1)
  assert.equal(chunkLimitFromTransceive(1), 1)
})

test('本体と端末の小さい方に合わせる', () => {
  assert.equal(effectiveChunkSize(200, 116), 116, '端末の方が小さい')
  assert.equal(effectiveChunkSize(64, MAX_CHUNK_SIZE), 64, '本体の方が小さい')
  assert.equal(effectiveChunkSize(500, 500), MAX_CHUNK_SIZE, 'どちらも大きい')
})

test('進んだら数え直す', () => {
  assert.equal(nextStalledCount(100, 50, 2), 0, '50 から 100 まで進んだ')
})

test('進まなければ数え上げる', () => {
  assert.equal(nextStalledCount(50, 50, 0), 1)
  assert.equal(nextStalledCount(50, 50, 1), 2)
})

test('進まない状態が続いたらあきらめる', () => {
  assert.equal(shouldGiveUp(MAX_STALLED - 1), false)
  assert.equal(shouldGiveUp(MAX_STALLED), true)
})

test('少しずつでも進んでいる限り続ける', () => {
  // かざし直しをくり返して進む作りなので、途中で切れても止めない
  let stalled = 0
  for (const [before, sent] of [[0, 100], [100, 100], [100, 300], [300, 300], [300, 900]]) {
    stalled = nextStalledCount(sent, before, stalled)
    assert.equal(shouldGiveUp(stalled), false)
  }
})

test('一度も送れていない失敗はかざし直しても直らない', () => {
  // 相手が違う、受け取る画面でない、画像が大きすぎる、など
  assert.equal(isFatal(new NfcError('本体で [NFC] を選んでから送ってください'), 0), true)
})

test('途中まで送れていたら電波が切れただけとみなす', () => {
  assert.equal(isFatal(new NfcError('本体からの応答を読み取れませんでした'), 4096), false)
})

test('通信そのものの例外はかざし直しの対象', () => {
  // react-native-nfc-manager が投げるものは NfcError ではない
  assert.equal(isFatal(new Error('Tag was lost'), 0), false)
  assert.equal(isFatal(new NfcCancelled('送信をやめました'), 0), false)
})
