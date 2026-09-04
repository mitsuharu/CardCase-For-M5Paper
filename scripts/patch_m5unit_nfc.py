"""M5Unit-NFC の NFC-A エミュレーションの受信バッファを広げる。

ライブラリの受信バッファは 64 バイト固定だが、こちらのやり取りは
1 フレーム 253 バイトまで使う。そのままでは受け取れない。

さらに、受け取った長さを確かめずにバッファへ読み込む箇所があり、
64 バイトを超えるフレームが届くとメモリを壊す。あわせて塞ぐ。

同じ内容で何度実行しても結果が変わらないようにしてある。
ライブラリを更新して対象の記述が見つからなくなった場合は、
黙って通さずビルドを止める。気付かずに壊れた状態で動くのを避けるため。
"""

import sys
from pathlib import Path

Import("env")  # noqa: F821

TARGET = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") \
    / "M5Unit-NFC" / "src" / "nfc" / "layer" / "a" / "emulation_layer_a_ST25R3916.cpp"

# 1 フレームの上限 253 バイトを収められる大きさ
BUFFER_FROM = "uint8_t rx[64]{};"
BUFFER_TO = "uint8_t rx[256]{};"

# 長さを確かめずに読み込んでいる箇所
GUARD_ANCHOR = """        _u.readFIFOSize(bytes, bits);
        rx_len = bytes;
"""
GUARD_PATCHED = """        _u.readFIFOSize(bytes, bits);
        rx_len = bytes;

        // CardCase: バッファに収まらないフレームは読み込む前に捨てる
        if (rx_len > sizeof(rx)) {
            _u.writeDirectCommand(CMD_CLEAR_FIFO);
            _u.writeDirectCommand(CMD_UNMASK_RECEIVE_DATA);
            return EmulationLayerA::State::Active;
        }
"""


def fail(message):
    print("patch_m5unit_nfc: " + message)
    sys.exit(1)


def main():
    if not TARGET.is_file():
        # ライブラリの取得前に走ることがある。次のビルドで当たる。
        return

    source = TARGET.read_text()

    if BUFFER_TO in source and "CardCase:" in source:
        return  # 適用済み

    if BUFFER_FROM not in source:
        fail("受信バッファの記述が見つかりません。M5Unit-NFC の更新で構造が変わった可能性があります: %s" % TARGET)
    if GUARD_ANCHOR not in source:
        fail("長さを確かめる箇所が見つかりません。M5Unit-NFC の更新で構造が変わった可能性があります: %s" % TARGET)

    source = source.replace(BUFFER_FROM, BUFFER_TO)
    source = source.replace(GUARD_ANCHOR, GUARD_PATCHED, 1)
    TARGET.write_text(source)
    print("patch_m5unit_nfc: 受信バッファを 256 バイトに広げました")


main()
