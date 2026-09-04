/**
 * 1 回に送る大きさと、切れたときにどうするかの判断。
 *
 * 実機の API を使わない計算だけなので、ここに分けて試験できるようにしている。
 * 送信そのものは nfcSender.ts にある。
 */

// 拡張子まで書くのは、この 2 つが node の試験からも読み込まれるため。
// Metro は省略しても解決するが、node は解決しない。
import { NfcError } from './errors.ts'
import { HEADER_SIZE, MAX_CHUNK_SIZE } from './protocol.ts'

/** 一度も進まないまま何回続いたらあきらめるか */
export const MAX_STALLED = 3

/**
 * 端末が 1 回で送れる長さから、本体に渡せる中身の大きさを出す。
 *
 * Android は端末ごとに上限が違う。253 バイトを扱えない端末があるので、
 * 繋いだあとに確かめた値をここに渡す。取れなかったときは決まりの上限を使う。
 * 送るのは見出しと位置を含めた全体なので、その分を引く。
 */
export function chunkLimitFromTransceive(maxTransceiveLength: number | null | undefined): number {
  if (!maxTransceiveLength || maxTransceiveLength <= 0) {
    return MAX_CHUNK_SIZE
  }
  return Math.max(1, Math.min(MAX_CHUNK_SIZE, maxTransceiveLength - (HEADER_SIZE + 4 + 4)))
}

/** 本体と端末のどちらの上限にも収まる大きさにする */
export function effectiveChunkSize(deviceMaxChunkSize: number, transceiveLimit: number): number {
  return Math.min(deviceMaxChunkSize, MAX_CHUNK_SIZE, transceiveLimit)
}

/**
 * 一度も進まない状態が何回続いたかを数える。
 *
 * 進んだなら数え直す。かざし直しで少しずつ進んでいる限りは続ける。
 */
export function nextStalledCount(sent: number, before: number, stalled: number): number {
  return sent > before ? 0 : stalled + 1
}

/** 繋がっていないか、設定が違う。かざし直しても直らない。 */
export function shouldGiveUp(stalled: number): boolean {
  return stalled >= MAX_STALLED
}

/**
 * 繋ぎ直しても直らない失敗か。
 *
 * 1 バイトも送れていない状態での NfcError は、相手が違う、
 * 受け取る画面でない、画像が大きすぎるなど、かざし直しでは変わらないもの。
 * 途中まで送れていたなら電波が切れただけなので、続きから送り直す。
 */
export function isFatal(error: unknown, sent: number): boolean {
  return error instanceof NfcError && sent === 0
}
