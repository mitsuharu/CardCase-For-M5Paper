import * as ImageManipulator from 'expo-image-manipulator';
import * as ImagePicker from 'expo-image-picker';
import { File } from 'expo-file-system';

import { fitToScreen } from './fit';

/** 送る画像。バイト列と、確認用の表示先。 */
export type PreparedImage = {
  bytes: Uint8Array;
  uri: string;
  width: number;
  height: number;
};

/**
 * 画面に収まる大きさまで縮めて JPEG にする。
 *
 * NFC は速くないので、送る量がそのまま待ち時間になる。
 * 本体の階調は 4 段階しかないため、品質を落としても見た目に響かない。
 *
 * 縮める先は本体が実際に使う表示領域に合わせる。本体は画像の向きに応じて
 * 画面を回すので、枠の縦横は画像が横長か縦長かで変わる。
 * 回転そのものは本体側に任せる。
 */
export async function prepareImage(
  uri: string,
  screenWidth: number,
  screenHeight: number,
  maxBytes: number,
): Promise<PreparedImage> {
  // 元の大きさを知るために一度だけ読む
  const source = await ImageManipulator.ImageManipulator.manipulate(uri).renderAsync();

  // 縮小は一度だけ。大きさの決め方は fit.ts にある。
  const { width, height } = fitToScreen(source.width, source.height, screenWidth, screenHeight);

  const rendered = await ImageManipulator.ImageManipulator.manipulate(uri)
    .resize({ width, height })
    .renderAsync();

  let smallest: PreparedImage | null = null;

  const measure = async (saved: { uri: string; width: number; height: number }) => {
    const bytes = await new File(saved.uri).bytes();
    const candidate: PreparedImage = {
      bytes,
      uri: saved.uri,
      width: saved.width,
      height: saved.height,
    };
    if (!smallest || bytes.length < smallest.bytes.length) {
      smallest = candidate;
    }
    return candidate;
  };

  // まず PNG を試す。文字や図の画像は JPEG より小さくなる上に劣化しない。
  // 名刺のような画像が主な用途なので、収まるならこちらを使う。
  const png = await measure(
    await rendered.saveAsync({ format: ImageManipulator.SaveFormat.PNG }),
  );
  if (png.bytes.length <= maxBytes) {
    return png;
  }

  // 収まらないのは写真のような画像。ここからは品質だけで調整する。
  // 本体は白黒 4 階調しか出せないので、品質を落としても見た目には響かない。
  const qualities = [0.7, 0.5, 0.35, 0.25, 0.15, 0.05];

  for (const quality of qualities) {
    const candidate = await measure(
      await rendered.saveAsync({
        compress: quality,
        format: ImageManipulator.SaveFormat.JPEG,
      }),
    );
    if (candidate.bytes.length <= maxBytes) {
      return candidate;
    }
  }

  // 目安を超えても送れないわけではない。時間がかかることは呼び出し側が伝える。
  if (smallest) {
    return smallest;
  }
  throw new Error('この画像を読み込めませんでした');
}

export async function pickImage(): Promise<string | null> {
  const permission = await ImagePicker.requestMediaLibraryPermissionsAsync();
  if (!permission.granted) {
    throw new Error('写真へのアクセスが許可されていません');
  }

  const result = await ImagePicker.launchImageLibraryAsync({
    mediaTypes: ['images'],
    quality: 1,
  });
  if (result.canceled || result.assets.length === 0) {
    return null;
  }
  return result.assets[0].uri;
}

export async function takePhoto(): Promise<string | null> {
  const permission = await ImagePicker.requestCameraPermissionsAsync();
  if (!permission.granted) {
    throw new Error('カメラへのアクセスが許可されていません');
  }

  const result = await ImagePicker.launchCameraAsync({ quality: 1 });
  if (result.canceled || result.assets.length === 0) {
    return null;
  }
  return result.assets[0].uri;
}
