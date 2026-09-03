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
        p.buttonCount = 3; // ロータリスイッチの上/押込/下が BtnA/B/C に割り当たる
        p.palette = DevicePalette::Gray16;
        p.menuTextSize = 8;
        break;

    case DeviceKind::PaperS3:
        p.name = "M5PaperS3";
        p.width = 540;
        p.height = 960;
        p.imageRotation = DeviceRotation::Down; // 筐体のフックに合わせて上下反転
        p.hasTouch = true;
        p.buttonCount = 0; // 物理ボタンは電源ボタンのみ
        p.palette = DevicePalette::Gray16;
        p.menuTextSize = 8;
        break;

    case DeviceKind::PaperColor:
        p.name = "M5PaperColor";
        p.width = 400;
        p.height = 600;
        p.imageRotation = DeviceRotation::Up;
        p.hasTouch = false; // タッチパネル非搭載。ボタンだけが操作手段になる
        p.buttonCount = 3;
        p.palette = DevicePalette::Color6;
        p.menuTextSize = 5;
        break;

    case DeviceKind::PaperMono:
        p.name = "M5PaperMono";
        p.width = 480;
        p.height = 800;
        p.imageRotation = DeviceRotation::Up;
        p.hasTouch = true;
        p.buttonCount = 2;
        p.palette = DevicePalette::Gray4;
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
