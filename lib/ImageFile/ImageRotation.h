#pragma once

/**
 * 画像を表示するときに画面をどれだけ回すかを決める。
 *
 * 判断の材料は 2 つある。
 *   1. EXIF の向き … スマホの写真は縦向きに撮ってもピクセルは横長のまま保存され、
 *      向きは EXIF にしか入っていない
 *   2. 画面との向き合わせ … 横長の写真を縦長の画面にそのまま出すと細い帯になる
 *
 * 計算だけの純粋なロジックなので native 環境で単体テストする。
 */
namespace ImageFile
{
    /// 向きを揃えるときに回す向き（反時計回りに 90 度 = 時計回りに 3 回）
    const int kFitRotationSteps = 3;

    /**
     * 画像と画面の向きが食い違っているときに、追加で回す回数を返す。
     * 揃っている場合は 0、食い違っている場合は kFitRotationSteps。
     *
     * EXIF による回転と違ってどちら向きに回しても収まるが、
     * 本体を反時計回りに傾けて見る向きに合わせている。
     */
    int orientationFitSteps(int imageWidth, int imageHeight, int screenWidth, int screenHeight);

    /**
     * 画像を表示するときの画面の回転（0〜3）を返す。
     *
     * imageWidth / imageHeight はファイルに入っているままのピクセル寸法。
     * 0 を渡すと寸法が読めなかったものとして、EXIF の向きだけで決める。
     *
     * screenWidth / screenHeight は回転していない状態の画面の大きさ。
     * 画面を回しても物理的な向きは変わらないので、ここは入れ替えずに渡すこと。
     */
    int displayRotation(int orientation, int imageWidth, int imageHeight,
                        int baseRotation, int screenWidth, int screenHeight);
}
