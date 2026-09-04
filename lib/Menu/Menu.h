#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
#include <functional>
using String = std::string;
#endif

#include <DeviceProfile.h>
#include "Selection/Selection.h"

/// 一覧の項目の種類
enum class MenuItemKind
{
    Image,    // SD 内の画像。value は画像のパス
    Transfer, // WiFi で画像を受け取る
    Nfc,      // NFC で画像を受け取る
};

/// 一覧に載せる 1 項目
struct MenuItem
{
    MenuItemKind kind = MenuItemKind::Image;
    String title; // 画面に表示する文字列
    String value; // 選ばれたときに呼び出し側へ渡す値（画像のパスなど）
};

/**
 * ファイル一覧の表示と選択。
 *
 * タッチのある機種は行を直接タップして選ぶ。
 * タッチのない M5PaperColor では物理ボタンだけで操作するため、
 * 選択行を白黒反転で示しながら移動する。
 */
class Menu
{
    typedef std::function<void(const MenuItem &item)> SelectHandler;

public:
    static const int kMaxItems = 32;

    /// 項目を追加する。上限を超えた場合は false を返す。
    bool addItem(MenuItemKind kind, const String &title, const String &value);

    int itemCount() const { return _itemCount; }

    /// 項目を空にする。SD の中身が変わったときに作り直すために使う。
    void clear() { _itemCount = 0; }

    /// プロファイルに合わせて 1 ページの件数を決め、一覧を描画する
    void begin(const DeviceProfile &profile, int topY, SelectHandler onSelect);

    /// loop() から呼ぶ。タッチとボタンの入力を処理する。
    void update();

    /// 一覧を描き直す
    void render();

private:
    MenuItem _items[kMaxItems];
    int _itemCount = 0;

    DeviceProfile _profile;
    Selection _selection;
    SelectHandler _onSelect = nullptr;

    int _topY = 0;
    int _rowHeight = 0;
    int _footerTextSize = 2;
    bool _useCursor = false; // ボタン操作のカーソルを表示するか

    // 更新の遅い機種で、選択が動いてから描き直すまでの猶予
    bool _redrawPending = false;
    unsigned long _redrawAt = 0;

    String operationGuide() const;
    void requestRedraw(int previousIndex, int previousPage);

    void drawRow(int index);
    void confirm();
};
