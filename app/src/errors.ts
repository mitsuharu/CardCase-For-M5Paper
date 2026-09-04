/**
 * 送信で投げる例外。
 *
 * 実機の API を読み込まずに済むよう、ここに分けてある。
 * 判断だけを取り出したところ（chunking.ts）から参照するため。
 */

/** 送れなかった。利用者に伝える。 */
export class NfcError extends Error {}

/** 利用者が自分でやめたとき。失敗として扱わない。 */
export class NfcCancelled extends Error {}
