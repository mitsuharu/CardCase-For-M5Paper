/**
 * 画像を画面に収める大きさを決める。
 *
 * 実機の API を使わない計算だけなので、ここに分けて試験できるようにしている。
 */

export type Size = {
  width: number
  height: number
}

/**
 * 画面に収まる大きさを返す。
 *
 * 本体は画像の向きに応じて画面を回すので、実際に表示される枠は
 * 画像が横長か縦長かで縦横が入れ替わる。回転そのものは本体側に任せる。
 *
 * 拡大はしない。画面より大きくしても表示に使えず、
 * 画面より小さくすると無駄に粗くなるので、ここが唯一の正解になる。
 */
export function fitToScreen(
  sourceWidth: number,
  sourceHeight: number,
  screenWidth: number,
  screenHeight: number,
): Size {
  const screenIsLandscape = screenWidth > screenHeight
  const imageIsLandscape = sourceWidth > sourceHeight
  const boxWidth = imageIsLandscape === screenIsLandscape ? screenWidth : screenHeight
  const boxHeight = imageIsLandscape === screenIsLandscape ? screenHeight : screenWidth

  const scale = Math.min(1, boxWidth / sourceWidth, boxHeight / sourceHeight)
  return {
    width: Math.max(1, Math.round(sourceWidth * scale)),
    height: Math.max(1, Math.round(sourceHeight * scale)),
  }
}
