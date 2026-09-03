#include "DeviceProfile.h"

DeviceProfile profileFor(DeviceKind kind)
{
    DeviceProfile p;
    p.kind = kind;

    switch (kind)
    {
    case DeviceKind::M5Paper:
        p.name = "M5Paper";
        p.width = 540;
        p.height = 960;
        p.imageRotation = DeviceRotation::Up;
        p.hasTouch = true;
        // ロータリスイッチの 上 / 押込 / 下 がそのまま BtnA / BtnB / BtnC に並ぶ
        p.buttonA = ButtonRole::Prev;
        p.buttonB = ButtonRole::Select;
        p.buttonC = ButtonRole::Next;
        p.palette = DevicePalette::Gray16;
        p.menuTextSize = 8;
        break;

    case DeviceKind::PaperS3:
        p.name = "M5PaperS3";
        p.width = 540;
        p.height = 960;
        p.imageRotation = DeviceRotation::Down; // 筐体のフックに合わせて上下反転
        p.hasTouch = true;
        // 物理ボタンは電源ボタンのみなので、一覧の操作はタッチに任せる
        p.palette = DevicePalette::Gray16;
        p.menuTextSize = 8;
        break;

    case DeviceKind::PaperColor:
        p.name = "M5PaperColor";
        p.width = 400;
        p.height = 600;
        p.imageRotation = DeviceRotation::Up;
        p.hasTouch = false; // タッチパネル非搭載。ボタンだけが操作手段になる
        // 上下のボタンが BtnA / BtnB に隣り合い、3 つ目の BtnC が離れている。
        // 位置どおりに 上 / 下 を並べ、決定は離れた BtnC に置く。
        p.buttonA = ButtonRole::Prev;
        p.buttonB = ButtonRole::Next;
        p.buttonC = ButtonRole::Select;
        p.palette = DevicePalette::Color6;
        p.slowRefresh = true; // 1 画面 15〜30 秒
        p.menuTextSize = 5;
        break;

    case DeviceKind::PaperMono:
        p.name = "M5PaperMono";
        p.width = 480;
        p.height = 800;
        p.imageRotation = DeviceRotation::Up;
        p.hasTouch = true;
        // ボタンが 2 つしかないので、送りと決定に絞る
        p.buttonA = ButtonRole::Next;
        p.buttonB = ButtonRole::Select;
        p.palette = DevicePalette::Gray4;
        // 4 階調しか出ないので、反転を繰り返す波形に見合う画質差が出ない。
        // 点滅を抑えるほうを取る。
        p.imageRefresh = RefreshMode::Fast;
        p.hasFrontlight = true;
        p.hasNfc = true;
        p.menuTextSize = 6;
        break;

    case DeviceKind::Unknown:
    default:
        // 既定値のまま返す
        break;
    }

    return p;
}

#ifdef ARDUINO

#include <M5Unified.h>

DeviceKind currentDeviceKind()
{
    switch (M5.getBoard())
    {
    case m5::board_t::board_M5Paper:
        return DeviceKind::M5Paper;
    case m5::board_t::board_M5PaperS3:
        return DeviceKind::PaperS3;
    case m5::board_t::board_M5PaperColor:
        return DeviceKind::PaperColor;
    case m5::board_t::board_M5PaperMono:
        return DeviceKind::PaperMono;
    default:
        return DeviceKind::Unknown;
    }
}

DeviceProfile currentProfile()
{
    return profileFor(currentDeviceKind());
}

#endif
