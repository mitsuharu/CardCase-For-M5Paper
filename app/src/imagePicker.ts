import * as ImageManipulator from 'expo-image-manipulator';
import * as ImagePicker from 'expo-image-picker';
import { File } from 'expo-file-system';

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
  const screenIsLandscape = screenWidth > screenHeight;

  // 本体は白黒 4 階調なので、色や細かい階調は残しても表示に出ない。
  // NFC は速くないため、送る量を減らすほうが効く。
  let quality = 0.6;
  let scale = 1;

  for (let attempt = 0; attempt < 5; attempt++) {
    const context = ImageManipulator.ImageManipulator.manipulate(uri);
    const image = await context.renderAsync();

    const imageIsLandscape = image.width > image.height;
    const boxWidth = imageIsLandscape === screenIsLandscape ? screenWidth : screenHeight;
    const boxHeight = imageIsLandscape === screenIsLandscape ? screenHeight : screenWidth;

    const fit = Math.min(1, boxWidth / image.width, boxHeight / image.height) * scale;
    const width = Math.max(1, Math.round(image.width * fit));
    const height = Math.max(1, Math.round(image.height * fit));

    const resized = ImageManipulator.ImageManipulator.manipulate(uri).resize({ width, height });
    const rendered = await resized.renderAsync();
    const saved = await rendered.saveAsync({
      compress: quality,
      format: ImageManipulator.SaveFormat.JPEG,
    });

    const bytes = await new File(saved.uri).bytes();
    if (bytes.length <= maxBytes) {
      return { bytes, uri: saved.uri, width: saved.width, height: saved.height };
    }

    // まず品質を落とし、それでも収まらなければ小さくする
    if (quality > 0.3) {
      quality -= 0.1;
    } else {
      scale *= 0.75;
    }
  }

  throw new Error('この画像は大きすぎて送れません');
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
