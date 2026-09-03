#include "SdPins.h"

bool SdPins::isValid() const
{
    return clk >= 0 && cmd >= 0 && d0 >= 0 && d3 >= 0;
}

bool SdPins::usesSdmmc() const
{
    return isValid() && d1 >= 0 && d2 >= 0;
}

int SdPins::pullUpPins(int8_t *out, int capacity) const
{
    if (out == nullptr || !usesSdmmc())
    {
        return 0;
    }

    // クロック以外のデータ線とコマンド線。順番は配線図に合わせている。
    const int8_t candidates[] = {cmd, d0, d1, d2, d3};

    int count = 0;
    for (int i = 0; i < kMaxPullUpPins && count < capacity; i++)
    {
        if (candidates[i] >= 0)
        {
            out[count++] = candidates[i];
        }
    }
    return count;
}
