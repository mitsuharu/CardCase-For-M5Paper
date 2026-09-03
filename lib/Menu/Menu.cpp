#include "Menu.h"

#ifdef ARDUINO

#include <M5Unified.h>

namespace
{
    // フッタ（ページ表示と操作ガイド）の文字サイズの上限と下限
    const int kFooterTextSizeMax = 4;
    const int kFooterTextSizeMin = 2;

    // 更新の遅い機種で、ボタンが止まってから描き直すまでの待ち時間。
    // 連打しても最後の 1 回しか画面を更新しないようにする。
    //
    // 画面の更新中は処理がブロックされてボタンを取りこぼすため、
    // 目的の位置まで押し切れる程度の余裕を持たせている。
    const unsigned long kSlowRedrawDelayMs = 1200;
}

bool Menu::addItem(MenuItemKind kind, const String &title, const String &value)
{
    if (_itemCount >= kMaxItems)
    {
        return false;
    }
    _items[_itemCount].kind = kind;
    _items[_itemCount].title = title;
    _items[_itemCount].value = value;
    _itemCount++;
    return true;
}

namespace
{
    /// 役割を画面に出す短いラベルにする
    const char *roleLabel(ButtonRole role)
    {
        switch (role)
        {
        case ButtonRole::Prev:
            return "Up";
        case ButtonRole::Next:
            return "Down";
        case ButtonRole::Select:
            return "OK";
        default:
            return nullptr;
        }
    }

    void appendRole(String &guide, const char *name, ButtonRole role)
    {
        const char *label = roleLabel(role);
        if (label == nullptr)
        {
            return;
        }
        if (guide.length() > 0)
        {
            guide += "  ";
        }
        guide += name;
        guide += ":";
        guide += label;
    }
}

/// 操作ガイドは役割から組み立てる。機種ごとに割り当てが違うため。
String Menu::operationGuide() const
{
    if (!_profile.hasButtons())
    {
        return String("Touch file name!");
    }

    // 画面の幅が狭い機種でも 1 行に収めたいので短く書く
    String guide;
    appendRole(guide, "A", _profile.buttonA);
    appendRole(guide, "B", _profile.buttonB);
    appendRole(guide, "C", _profile.buttonC);
    return guide;
}

void Menu::begin(const DeviceProfile &profile, int topY, SelectHandler onSelect)
{
    _profile = profile;
    _topY = topY;
    _onSelect = onSelect;

    // ボタンで動かせる機種では、いまどれを選んでいるかを必ず見せる。
    // タッチと併用できる機種（M5Paper）でもカーソルが無いとボタン操作が成立しない。
    _useCursor = profile.hasButtons();

    M5.Display.setTextSize(profile.menuTextSize);
    _rowHeight = M5.Display.fontHeight();
    if (_rowHeight <= 0)
    {
        _rowHeight = profile.menuTextSize * 8;
    }

    // 操作ガイドは折り返すと読みにくいので、幅に収まる最大の文字サイズを選ぶ。
    // 画面の幅が機種ごとに違うため、固定値にはできない。
    String guide = operationGuide();
    int guideWidth = M5.Display.width() - _profile.margin() * 2;
    _footerTextSize = kFooterTextSizeMin;
    for (int size = kFooterTextSizeMax; size >= kFooterTextSizeMin; size--)
    {
        M5.Display.setTextSize(size);
        if (M5.Display.textWidth(guide.c_str()) <= guideWidth)
        {
            _footerTextSize = size;
            break;
        }
    }

    // フッタの分を残して 1 ページの件数を決める
    M5.Display.setTextSize(_footerTextSize);
    int footerHeight = M5.Display.fontHeight() * 2;
    int listHeight = M5.Display.height() - _topY - footerHeight;

    int visibleCount = listHeight / _rowHeight;
    if (visibleCount < 1)
    {
        visibleCount = 1;
    }

    _selection = Selection(_itemCount, visibleCount);

    render();
}

void Menu::drawRow(int index)
{
    int row = _selection.rowOf(index);
    if (row < 0)
    {
        return;
    }

    bool isSelected = _useCursor && index == _selection.selectedIndex;
    uint16_t fg = isSelected ? TFT_WHITE : TFT_BLACK;
    uint16_t bg = isSelected ? TFT_BLACK : TFT_WHITE;

    int y = _topY + row * _rowHeight;

    M5.Display.fillRect(0, y, M5.Display.width(), _rowHeight, bg);
    M5.Display.setTextSize(_profile.menuTextSize);
    M5.Display.setTextColor(fg, bg);
    M5.Display.setTextWrap(false);
    M5.Display.setCursor(_profile.margin(), y);
    M5.Display.print(_items[index].title.c_str());
    M5.Display.setTextWrap(true);
}

void Menu::render()
{
    // 描画モードは呼び出し側が決める。
    // 画像から戻ってきたときは残像を消したいので画質を優先し、
    // カーソル移動のときは速さを優先する、というように場面で変わるため。

    // 電子ペーパーは endWrite のたびに画面を更新する。
    // 行ごとに更新すると 1 行ずつ待たされるので、一覧全体を 1 回にまとめる。
    M5.Display.startWrite();

    int listHeight = _rowHeight * _selection.visibleCount;
    M5.Display.fillRect(0, _topY, M5.Display.width(), listHeight, TFT_WHITE);

    int start = _selection.pageStart();
    for (int i = 0; i < _selection.pageLength(); i++)
    {
        drawRow(start + i);
    }

    // フッタ：ページ位置と操作ガイド
    int footerY = _topY + listHeight;
    M5.Display.setTextSize(_footerTextSize);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.fillRect(0, footerY, M5.Display.width(), M5.Display.height() - footerY, TFT_WHITE);
    M5.Display.setCursor(_profile.margin(), footerY);

    if (_selection.pageCount() > 1)
    {
        M5.Display.printf("page %d/%d\n", _selection.pageIndex() + 1, _selection.pageCount());
    }
    else
    {
        M5.Display.println("");
    }

    M5.Display.setCursor(_profile.margin(), M5.Display.getCursorY());
    M5.Display.println(operationGuide().c_str());

    M5.Display.endWrite();

    _redrawPending = false;
}

/**
 * 選択が動いたことを受けて画面を描き直す。
 *
 * M5PaperColor は 1 回の更新に十数秒かかるうえ、部分更新が無く毎回全面を
 * 転送するので、行だけ描き直しても速くならない。ボタンが止まるまで待ってから
 * 1 回だけ描き直す。連打しても更新は 1 回で済む。
 */
void Menu::requestRedraw(int previousIndex, int previousPage)
{
    if (_profile.slowRefresh)
    {
        _redrawPending = true;
        _redrawAt = millis() + kSlowRedrawDelayMs;
        return;
    }

    // カーソルを追うだけなので速さを優先する
    if (M5.Display.isEPD())
    {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    }

    if (_selection.pageIndex() != previousPage)
    {
        render();
        return;
    }

    // 同じページ内の移動なら 2 行だけ描き直す
    M5.Display.startWrite();
    drawRow(previousIndex);
    drawRow(_selection.selectedIndex);
    M5.Display.endWrite();
}

void Menu::confirm()
{
    if (_onSelect == nullptr || _itemCount <= 0)
    {
        return;
    }
    _onSelect(_items[_selection.selectedIndex]);
}

void Menu::update()
{
    if (_itemCount <= 0)
    {
        return;
    }

    int previousIndex = _selection.selectedIndex;
    int previousPage = _selection.pageIndex();
    bool moved = false;

    // どのボタンが何をするかは機種ごとに違うので、プロファイルの役割に従う
    ButtonRole pressed = ButtonRole::None;
    if (M5.BtnA.wasPressed())
    {
        pressed = _profile.buttonA;
    }
    else if (M5.BtnB.wasPressed())
    {
        pressed = _profile.buttonB;
    }
    else if (M5.BtnC.wasPressed())
    {
        pressed = _profile.buttonC;
    }

    switch (pressed)
    {
    case ButtonRole::Prev:
        _selection.movePrev();
        moved = true;
        break;

    case ButtonRole::Next:
        _selection.moveNext();
        moved = true;
        break;

    case ButtonRole::Select:
        confirm();
        return;

    default:
        break;
    }

    if (moved)
    {
        requestRedraw(previousIndex, previousPage);
        return;
    }

    // 待ち時間が過ぎたらまとめて描き直す
    if (_redrawPending && static_cast<long>(millis() - _redrawAt) >= 0)
    {
        if (M5.Display.isEPD())
        {
            M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        }
        render();
        return;
    }

    if (!_profile.hasTouch)
    {
        return;
    }

    auto touch = M5.Touch.getDetail();
    if (!touch.wasPressed())
    {
        return;
    }

    int listHeight = _rowHeight * _selection.visibleCount;
    if (touch.y < _topY || touch.y >= _topY + listHeight)
    {
        return;
    }

    int row = (touch.y - _topY) / _rowHeight;
    if (row < 0 || row >= _selection.pageLength())
    {
        return;
    }

    _selection.select(_selection.pageStart() + row);
    confirm();
}

#endif
