import assert from 'node:assert/strict'
import { test } from 'node:test'

import { fitToScreen } from '../src/fit.ts'

// M5PaperMono の表示領域
const SCREEN_WIDTH = 480
const SCREEN_HEIGHT = 800

test('縦長の画像は縦長の枠に収める', () => {
  const fit = fitToScreen(3000, 4000, SCREEN_WIDTH, SCREEN_HEIGHT)
  assert.deepEqual(fit, { width: 480, height: 640 })
})

test('横長の画像は枠の縦横を入れ替えて収める', () => {
  // 本体が画面を回すので、横長の画像には 800x480 の枠が当たる。
  // 入れ替えを忘れると 480 幅に押し込められ、無駄に小さくなる。
  const fit = fitToScreen(4000, 3000, SCREEN_WIDTH, SCREEN_HEIGHT)
  assert.deepEqual(fit, { width: 640, height: 480 })
})

test('画面より小さい画像は拡大しない', () => {
  const fit = fitToScreen(200, 300, SCREEN_WIDTH, SCREEN_HEIGHT)
  assert.deepEqual(fit, { width: 200, height: 300 })
})

test('縦横がぴったりなら変えない', () => {
  const fit = fitToScreen(480, 800, SCREEN_WIDTH, SCREEN_HEIGHT)
  assert.deepEqual(fit, { width: 480, height: 800 })
})

test('縦横の比を保つ', () => {
  const fit = fitToScreen(1000, 250, SCREEN_WIDTH, SCREEN_HEIGHT)
  assert.equal(fit.width / fit.height, 4)
})

test('極端に細長くても 0 にならない', () => {
  // 0 を渡すと縮小そのものが失敗する
  const fit = fitToScreen(10000, 3, SCREEN_WIDTH, SCREEN_HEIGHT)
  assert.ok(fit.width >= 1)
  assert.ok(fit.height >= 1)
})

test('正方形の画像でも収まる', () => {
  const fit = fitToScreen(2000, 2000, SCREEN_WIDTH, SCREEN_HEIGHT)
  assert.deepEqual(fit, { width: 480, height: 480 })
})

test('横長の画面の機種でも枠を選び分ける', () => {
  // 画面が横長なら、横長の画像がそのままの向きで当たる
  const landscape = fitToScreen(4000, 3000, 800, 480)
  assert.deepEqual(landscape, { width: 640, height: 480 })

  // 縦長の画像には枠が入れ替わって 480x800 が当たる
  const portrait = fitToScreen(3000, 4000, 800, 480)
  assert.deepEqual(portrait, { width: 480, height: 640 })
})
