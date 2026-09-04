import NfcManager, { NfcTech } from 'react-native-nfc-manager';
import { Platform } from 'react-native';

import {
  chunkLimitFromTransceive,
  effectiveChunkSize,
  isFatal,
  nextStalledCount,
  shouldGiveUp,
} from './chunking'
import { NfcCancelled, NfcError } from './errors'
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

export { NfcCancelled, NfcError }

let cancelRequested = false;

/**
 * 送信をやめる。
 *
 * iOS はシートに「キャンセル」があるが、Android は何も出ないので
 * アプリ側にやめる手段が要る。繋ぎ直しをくり返す作りなので、
 * 印を立てるだけでは止まらない。通信そのものも打ち切る。
 */
export function cancelSending(): void {
  cancelRequested = true;
  NfcManager.cancelTechnologyRequest().catch(() => undefined);
}

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

/**
 * Android では端末ごとに 1 回で送れる長さの上限が違う。
 * 253 バイトを扱えない端末があるので、繋いだあとに確かめる。
 * iOS にはこの API が無く、上限も十分あるので確かめない。
 */
async function deviceChunkLimit(): Promise<number> {
  if (Platform.OS !== 'android') {
    return MAX_CHUNK_SIZE;
  }
  try {
    const max = await NfcManager.getMaxTransceiveLength();
    if (!max || max <= 0) {
      return MAX_CHUNK_SIZE;
    }
    return chunkLimitFromTransceive(max);
  } catch {
    return MAX_CHUNK_SIZE;
  }
}

/** 本体の能力を尋ねる */
async function hello(): Promise<DeviceInfo> {
  const response = await send(Command.Hello, []);
  const status = responseStatus(response);

  if (status === Status.NotReady) {
    // 本体は待ち受けているが、受け取る画面になっていない
    throw new NfcError('本体で [NFC] を選んでから送ってください');
  }
  if (status !== Status.Ok) {
    throw new NfcError(`本体が応答しません (${statusName(status)})`);
  }
  return {
    maxChunkSize: readU16(response, 5),
    maxImageSize: readU32(response, 7),
    width: readU16(response, 11),
    height: readU16(response, 13),
  };
}

/**
 * 1 回のかざしで送れるところまで送る。
 *
 * 送り切れたら true、まだ残っているなら false を返す。
 * 電波が切れた場合は例外が飛ぶので、呼び出し側でかざし直してもらう。
 */
async function transfer(image: Uint8Array, onProgress: (p: Progress) => Promise<void>): Promise<void> {
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
  await onProgress({ sent: offset, total: image.length });

  const chunkSize = effectiveChunkSize(info.maxChunkSize, await deviceChunkLimit());

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
      await onProgress({ sent: offset, total: image.length });
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

/** かざし直しを何回まで促すか */
const MAX_ATTEMPTS = 30;

/**
 * 端末をかざしてもらい、画像を送る。
 *
 * NFC の通信距離は数 cm しかなく、少し動かすだけで切れる。
 * 数十 KB を一度で送り切るのは難しいので、切れたら繋ぎ直して
 * 受け取り済みの位置から続ける。位置は本体が返してくる。
 *
 * iOS ではシートを閉じずにポーリングだけやり直せる。閉じて開き直すと
 * 端末が振動して待たされるので、繋ぎ直しに気付かないくらい滑らかになる。
 *
 * 呼ぶ前に NfcManager.start() を済ませておくこと。
 */
export async function sendImage(image: Uint8Array, onProgress: (p: Progress) => void): Promise<void> {
  const tech = Platform.OS === 'ios' ? NfcTech.MifareIOS : NfcTech.NfcA;
  const ios = Platform.OS === 'ios';

  let sent = 0;
  let stalled = 0;
  let lastError: unknown = null;

  cancelRequested = false;

  // 前回の接続が残っていると次が始められない。
  // 送信中に画面を離れるなどで残ることがあるので、始める前に必ず落とす。
  await NfcManager.cancelTechnologyRequest().catch(() => undefined);

  // シートの文言を進み具合に合わせて書き換える。
  // 毎回書くと 1 通信ごとに往復が増えるので、変化が見える幅でだけ更新する。
  let shownPercent = -1;
  const report = async (p: Progress) => {
    if (cancelRequested) {
      throw new NfcCancelled('送信をやめました');
    }
    sent = p.sent;
    onProgress(p);

    if (!ios || p.total === 0) {
      return;
    }
    const percent = Math.round((p.sent / p.total) * 100);
    if (percent >= shownPercent + 5 || percent === 100) {
      shownPercent = percent;
      await NfcManager.setAlertMessageIOS(
        percent >= 100 ? '送信中 100%' : `送信中 ${percent}% ・ 離さないでください`,
      );
    }
  };

  await NfcManager.requestTechnology(tech, {
    alertMessage: 'M5Paper に近づけたまま待ってください',
  });

  if (Platform.OS === 'android') {
    // 既定の待ち時間は短く、本体の応答が間に合わないことがある
    await NfcManager.setTimeout(1000).catch(() => undefined);
  }

  try {
    for (let attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
      const before = sent;

      try {
        await transfer(image, report);

        if (ios) {
          await NfcManager.setAlertMessageIOS('送信しました');
        }
        return;
      } catch (e) {
        if (cancelRequested || e instanceof NfcCancelled) {
          throw new NfcCancelled('送信をやめました');
        }
        lastError = e;

        // 相手が違う、受け取る画面でないなど、繋ぎ直しても直らないものは即やめる
        if (isFatal(e, sent)) {
          throw e;
        }

        stalled = nextStalledCount(sent, before, stalled);
        if (shouldGiveUp(stalled)) {
          break;
        }
      }

      if (cancelRequested) {
        throw new NfcCancelled('送信をやめました');
      }

      if (ios) {
        // シートは開いたまま、探し直すところからやり直す
        await NfcManager.restartTechnologyRequestIOS();
      }
    }

    throw new NfcError(
      `送信が途中で切れました（${Math.round(sent / 1024)} KB まで送信）` +
        (lastError instanceof Error ? `: ${lastError.message}` : ''),
    );
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
