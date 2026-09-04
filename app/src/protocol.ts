/**
 * 本体とやり取りする決まり。
 *
 * ファームウェアの lib/NfcTransfer/Protocol と対になっている。
 * どちらかを変えたらもう一方も直すこと。
 */

export const MAGIC_0 = 0x43; // 'C'
export const MAGIC_1 = 0x43; // 'C'
export const VERSION = 1;

/**
 * 1 フレームの最大の長さ。
 * ISO14443-A のフレームは CRC を除いて 253 バイトまで。
 */
export const MAX_FRAME_SIZE = 253;

/** 要求と応答に共通する先頭部分（印 2 + 版 1 + コマンド 1） */
export const HEADER_SIZE = 4;

/** 応答は先頭部分に状態を 1 バイト足す */
export const RESPONSE_HEADER_SIZE = HEADER_SIZE + 1;

/** DATA で 1 回に送れる本体の最大。フレームの上限から見出しを引いた値。 */
export const MAX_CHUNK_SIZE = MAX_FRAME_SIZE - (HEADER_SIZE + 4 + 4);

export const Command = {
  Hello: 0x01,
  Begin: 0x02,
  Data: 0x03,
  Commit: 0x04,
  Abort: 0x05,
} as const;

export const Status = {
  Ok: 0x00,
  Accepted: 0x01,
  BadRequest: 0x10,
  BadOffset: 0x11,
  TooLarge: 0x12,
  NoTransfer: 0x13,
  CrcMismatch: 0x14,
  Incomplete: 0x15,
  NotReady: 0x16,
} as const;

export function statusName(status: number): string {
  const names: Record<number, string> = {
    [Status.Ok]: 'OK',
    [Status.Accepted]: 'ACCEPTED',
    [Status.BadRequest]: 'BAD_REQUEST',
    [Status.BadOffset]: 'BAD_OFFSET',
    [Status.TooLarge]: 'TOO_LARGE',
    [Status.NoTransfer]: 'NO_TRANSFER',
    [Status.CrcMismatch]: 'CRC_MISMATCH',
    [Status.Incomplete]: 'INCOMPLETE',
    [Status.NotReady]: 'NOT_READY',
  };
  return names[status] ?? `UNKNOWN(0x${status.toString(16)})`;
}

/** 大きい方が先の並びで 32bit を書く */
export function writeU32(value: number): number[] {
  return [
    (value >>> 24) & 0xff,
    (value >>> 16) & 0xff,
    (value >>> 8) & 0xff,
    value & 0xff,
  ];
}

/** 大きい方が先の並びで 32bit を読む */
export function readU32(bytes: number[] | Uint8Array, offset: number): number {
  return (
    ((bytes[offset] << 24) >>> 0) +
    (bytes[offset + 1] << 16) +
    (bytes[offset + 2] << 8) +
    bytes[offset + 3]
  );
}

/** 大きい方が先の並びで 16bit を読む */
export function readU16(bytes: number[] | Uint8Array, offset: number): number {
  return (bytes[offset] << 8) | bytes[offset + 1];
}

export function frame(command: number, body: number[] = []): number[] {
  return [MAGIC_0, MAGIC_1, VERSION, command, ...body];
}

/** 応答が自分あてのものか確かめる */
export function isValidResponse(response: number[] | Uint8Array, command: number): boolean {
  return (
    response.length >= RESPONSE_HEADER_SIZE &&
    response[0] === MAGIC_0 &&
    response[1] === MAGIC_1 &&
    response[2] === VERSION &&
    response[3] === command
  );
}

export function responseStatus(response: number[] | Uint8Array): number {
  return response[4];
}

/**
 * 画像の壊れを見つけるための検査値（IEEE 802.3 の CRC-32）。
 * 本体側の実装と同じ値になる必要がある。
 */
export function crc32(data: Uint8Array): number {
  let crc = 0xffffffff;
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i];
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}
