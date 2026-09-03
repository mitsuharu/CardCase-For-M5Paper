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

    /**
     * 画像と画面の向きが食い違っているときに、追加で回す回数（0 か 1）を返す。
     *
     * カメラの横長写真を縦長の画面にそのまま出すと、細い帯になって余白ばかりになる。
     * 向きを揃えると画面いっぱいに表示できる。
     */
    int orientationFitSteps(int imageWidth, int imageHeight, int screenWidth, int screenHeight);
}
