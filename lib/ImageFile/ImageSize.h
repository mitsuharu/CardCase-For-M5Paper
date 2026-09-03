#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * 画像の寸法を読み、画面の向きに合わせるための回転量を求める。
 *
 * バイト列を読むだけの純粋なロジックなので native 環境で単体テストする。
 */
namespace ImageFile
{
    /**
     * JPEG または PNG の先頭バイト列からピクセル寸法を読む。
     * 読めた場合だけ width / height に書き込んで true を返す。
     *
     * JPEG は APPn セグメントを読み飛ばして SOF から取る。EXIF にサムネイルが
     * 入っていると SOF は先頭から数十 KB 先になるので、data には十分な長さが要る。
     */
    bool imageSize(const uint8_t *data, size_t size, int *width, int *height);

    /// 向きを揃えるときに回す向き（反時計回りに 90 度 = 時計回りに 3 回）
    const int kFitRotationSteps = 3;

    /**
     * 画像と画面の向きが食い違っているときに、追加で回す回数を返す。
     * 揃っている場合は 0、食い違っている場合は kFitRotationSteps。
     *
     * カメラの横長写真を縦長の画面にそのまま出すと、細い帯になって余白ばかりになる。
     * 向きを揃えると画面いっぱいに表示できる。
     *
     * EXIF による回転と違ってどちら向きに回しても収まるが、
     * 本体を反時計回りに傾けて見る向き（反時計回り）に合わせている。
     */
    int orientationFitSteps(int imageWidth, int imageHeight, int screenWidth, int screenHeight);
}
