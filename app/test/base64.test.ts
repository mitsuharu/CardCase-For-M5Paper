import test from 'node:test'
import assert from 'node:assert/strict'

import { bytesFromDataUrl, decodeBase64 } from '../src/base64.ts'

test('よく知られた値に一致する', () => {
  assert.deepEqual(Array.from(decodeBase64('TWFu')), [0x4d, 0x61, 0x6e])
  assert.deepEqual(Array.from(decodeBase64('aGVsbG8=')), Array.from(Buffer.from('hello')))
  assert.deepEqual(Array.from(decodeBase64('aGVsbG8h')), Array.from(Buffer.from('hello!')))
})

test('詰め物の数が変わっても Buffer と同じになる', () => {
  for (let length = 0; length < 64; length++) {
    const source = Buffer.alloc(length)
    for (let i = 0; i < length; i++) {
      source[i] = (i * 37 + 11) & 0xff
    }
    const encoded = source.toString('base64')
    assert.deepEqual(
      Array.from(decodeBase64(encoded)),
      Array.from(source),
      `${length} バイト`,
    )
  }
})

test('PNG の先頭が壊れない', () => {
  // WebView から返ってくるのはこの形。署名が崩れると本体が形式を判断できない。
  const signature = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]
  const png = Buffer.from([...signature, 1, 2, 3, 4, 5])
  const dataUrl = `data:image/png;base64,${png.toString('base64')}`
  assert.deepEqual(Array.from(bytesFromDataUrl(dataUrl)), Array.from(png))
})

test('data URL でないものは投げる', () => {
  assert.throws(() => bytesFromDataUrl('https://example.com/a.png'))
  assert.throws(() => bytesFromDataUrl('data:image/png,notbase64'))
})

test('base64 でない文字は投げる', () => {
  assert.throws(() => decodeBase64('あいうえお'))
  assert.throws(() => decodeBase64('ab*d'))
})
