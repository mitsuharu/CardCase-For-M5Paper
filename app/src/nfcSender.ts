import NfcManager, { NfcTech } from 'react-native-nfc-manager';
import { Platform } from 'react-native';

import {
  Command,
  MAX_CHUNK_SIZE,
  Status,
  crc32,
  frame,
  isValidResponse,
  readU16,
  readU32,
  responseStatus,
  statusName,
  writeU32,
} from './protocol';

export type Progress = {
  sent: number;
  total: number;
};

export type DeviceInfo = {
  maxChunkSize: number;
  maxImageSize: number;
  width: number;
  height: number;
};

export class NfcError extends Error {}

/**
 * NFC でフレームを 1 往復させる。
 *
 * iOS と Android で API が違う。iOS は Core NFC の MiFare として扱い、
 * Android は NfcA として扱うが、どちらも生のフレームをやり取りしている。
 */
async function exchange(request: number[]): Promise<number[]> {
  if (Platform.OS === 'ios') {
    const response = await NfcManager.sendMifareCommandIOS(request);
    return Array.from(response);
  }
  const response = await NfcManager.transceive(request);
  return Array.from(response);
}

async function send(command: number, body: number[]): Promise<number[]> {
  const response = await exchange(frame(command, body));
  if (!isValidResponse(response, command)) {
    throw new NfcError('本体からの応答を読み取れませんでした');
  }
  return response;
}

/** 本体の能力を尋ねる */
async function hello(): Promise<DeviceInfo> {
  const response = await send(Command.Hello, []);
  if (responseStatus(response) !== Status.Ok) {
    throw new NfcError(`本体が応答しません (${statusName(responseStatus(response))})`);
  }
  return {
    maxChunkSize: readU16(response, 5),
    maxImageSize: readU32(response, 7),
    width: readU16(response, 11),
    height: readU16(response, 13),
  };
}

/**
 * 画像を送る。
 *
 * NFC は端末を離すと切れるので、そのときは同じ内容で始め直せば
 * 受け取り済みの位置から続けられる。本体側がその位置を返してくる。
 */
async function transfer(image: Uint8Array, onProgress: (p: Progress) => void): Promise<void> {
  const info = await hello();

  if (image.length > info.maxImageSize) {
    throw new NfcError(
      `画像が大きすぎます（${Math.round(image.length / 1024)}KB / 上限 ${Math.round(info.maxImageSize / 1024)}KB）`,
    );
  }

  const checksum = crc32(image);
  const begin = await send(Command.Begin, [...writeU32(image.length), ...writeU32(checksum)]);
  if (responseStatus(begin) !== Status.Accepted) {
    throw new NfcError(`転送を始められません (${statusName(responseStatus(begin))})`);
  }

  const transferId = readU32(begin, 5);
  let offset = readU32(begin, 9);
  onProgress({ sent: offset, total: image.length });

  // 本体が扱える大きさに合わせる。こちらの上限より小さいことがある。
  const chunkSize = Math.min(info.maxChunkSize, MAX_CHUNK_SIZE);

  while (offset < image.length) {
    const length = Math.min(chunkSize, image.length - offset);
    const body = [
      ...writeU32(transferId),
      ...writeU32(offset),
      ...Array.from(image.subarray(offset, offset + length)),
    ];

    const response = await send(Command.Data, body);
    const status = responseStatus(response);

    if (status === Status.Accepted || status === Status.BadOffset) {
      // ずれていた場合も本体が続きの位置を返すので、そこから送り直す
      offset = readU32(response, 5);
      onProgress({ sent: offset, total: image.length });
      continue;
    }
    throw new NfcError(`送信に失敗しました (${statusName(status)})`);
  }

  const commit = await send(Command.Commit, writeU32(transferId));
  const status = responseStatus(commit);
  if (status !== Status.Ok) {
    throw new NfcError(`確定できませんでした (${statusName(status)})`);
  }
}

/**
 * 端末をかざしてもらい、画像を送る。
 *
 * 呼ぶ前に NfcManager.start() を済ませておくこと。
 */
export async function sendImage(image: Uint8Array, onProgress: (p: Progress) => void): Promise<void> {
  const tech = Platform.OS === 'ios' ? NfcTech.MifareIOS : NfcTech.NfcA;

  try {
    await NfcManager.requestTechnology(tech, {
      alertMessage: 'M5Paper に近づけてください',
    });
    await transfer(image, onProgress);

    if (Platform.OS === 'ios') {
      await NfcManager.setAlertMessageIOS('送信しました');
    }
  } finally {
    // 例外が出ても必ず終了させる。残すと次回に繋がらなくなる。
    await NfcManager.cancelTechnologyRequest().catch(() => undefined);
  }
}

export async function isSupported(): Promise<boolean> {
  try {
    return await NfcManager.isSupported();
  } catch {
    return false;
  }
}
