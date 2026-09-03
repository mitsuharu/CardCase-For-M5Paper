#include <unity.h>
#include <SdPins/SdPins.h>

// SD の配線の判定。
//
// 実機のピン番号は M5Unified の _pin_table_sd から取ってきた実際の値を使う。
// 「D1 は D0 の隣だろう」と番号を計算して作ってしまうと、SPI の機種では
// クロックや MOSI、別の周辺回路のピンを掴んでしまう。それを防ぐためのテスト。

void setUp(void) {}
void tearDown(void) {}

namespace
{
    // M5Unified の _pin_table_sd より
    //                     clk, cmd, d0, d1, d2, d3
    const SdPins kM5Paper = {14, 12, 13, -1, -1, 4};
    const SdPins kPaperS3 = {39, 38, 40, -1, -1, 47};
    const SdPins kPaperColor = {15, 13, 14, -1, -1, 47};
    const SdPins kPaperMono = {13, 12, 11, 10, 9, 8};

    // データ線が連番でない配線。実機ではないが、番号を計算で作る実装を
    // 書いてしまったときにここで落ちる。
    const SdPins kScattered = {20, 21, 30, 4, 17, 33};

    const SdPins kAll[] = {kM5Paper, kPaperS3, kPaperColor, kPaperMono, kScattered};
    const int kAllCount = sizeof(kAll) / sizeof(kAll[0]);

    /// pin が配線として渡された番号のどれかであること
    bool isDeclaredPin(const SdPins &pins, int8_t pin)
    {
        return pin == pins.cmd || pin == pins.d0 || pin == pins.d1 || pin == pins.d2 || pin == pins.d3;
    }
}

void test_only_paper_mono_uses_sdmmc()
{
    // D1 / D2 が出ているのは M5PaperMono だけ
    TEST_ASSERT_TRUE(kPaperMono.usesSdmmc());

    TEST_ASSERT_FALSE(kM5Paper.usesSdmmc());
    TEST_ASSERT_FALSE(kPaperS3.usesSdmmc());
    TEST_ASSERT_FALSE(kPaperColor.usesSdmmc());
}

void test_all_devices_are_valid()
{
    for (int i = 0; i < kAllCount; i++)
    {
        TEST_ASSERT_TRUE(kAll[i].isValid());
    }
}

void test_pull_ups_are_only_for_sdmmc()
{
    int8_t out[SdPins::kMaxPullUpPins];

    // SPI の機種では何も触らない。既に動いている配線に手を入れない。
    TEST_ASSERT_EQUAL_INT(0, kM5Paper.pullUpPins(out, SdPins::kMaxPullUpPins));
    TEST_ASSERT_EQUAL_INT(0, kPaperS3.pullUpPins(out, SdPins::kMaxPullUpPins));
    TEST_ASSERT_EQUAL_INT(0, kPaperColor.pullUpPins(out, SdPins::kMaxPullUpPins));
}

void test_paper_mono_pull_ups_are_the_declared_data_lines()
{
    int8_t out[SdPins::kMaxPullUpPins];
    int count = kPaperMono.pullUpPins(out, SdPins::kMaxPullUpPins);

    TEST_ASSERT_EQUAL_INT(5, count);
    TEST_ASSERT_EQUAL_INT8(12, out[0]); // cmd
    TEST_ASSERT_EQUAL_INT8(11, out[1]); // d0
    TEST_ASSERT_EQUAL_INT8(10, out[2]); // d1
    TEST_ASSERT_EQUAL_INT8(9, out[3]);  // d2
    TEST_ASSERT_EQUAL_INT8(8, out[4]);  // d3
}

void test_never_touches_undeclared_pins()
{
    // ここが本題。返すピンは必ず配線として渡された番号のいずれかで、
    // 計算で作った番号やクロックが混じらないこと。
    //
    // 以前 D1 / D2 を「D0 の隣」として計算していたことがあり、
    // M5PaperS3 では 39(SCLK) と 38(MOSI)、M5PaperColor では 12(EPD RST) を
    // 掴んでしまっていた。kScattered はその書き方を検出するための配線。
    int8_t out[SdPins::kMaxPullUpPins];

    for (int i = 0; i < kAllCount; i++)
    {
        const SdPins &pins = kAll[i];
        int count = pins.pullUpPins(out, SdPins::kMaxPullUpPins);

        for (int n = 0; n < count; n++)
        {
            TEST_ASSERT_TRUE(isDeclaredPin(pins, out[n]));
            TEST_ASSERT_NOT_EQUAL_INT8(pins.clk, out[n]);
            TEST_ASSERT_TRUE(out[n] >= 0);
        }
    }
}

void test_data_lines_need_not_be_consecutive()
{
    // 連番を前提にしないこと。返るのは宣言された番号そのもの。
    int8_t out[SdPins::kMaxPullUpPins];
    int count = kScattered.pullUpPins(out, SdPins::kMaxPullUpPins);

    TEST_ASSERT_EQUAL_INT(5, count);
    TEST_ASSERT_EQUAL_INT8(21, out[0]); // cmd
    TEST_ASSERT_EQUAL_INT8(30, out[1]); // d0
    TEST_ASSERT_EQUAL_INT8(4, out[2]);  // d1
    TEST_ASSERT_EQUAL_INT8(17, out[3]); // d2
    TEST_ASSERT_EQUAL_INT8(33, out[4]); // d3
}

void test_incomplete_wiring_is_rejected()
{
    // ピンが揃っていない場合は使わない
    SdPins empty;
    TEST_ASSERT_FALSE(empty.isValid());
    TEST_ASSERT_FALSE(empty.usesSdmmc());

    int8_t out[SdPins::kMaxPullUpPins];
    TEST_ASSERT_EQUAL_INT(0, empty.pullUpPins(out, SdPins::kMaxPullUpPins));

    // D1 だけ出ていても 4bit にはならない
    SdPins halfWired = {13, 12, 11, 10, -1, 8};
    TEST_ASSERT_FALSE(halfWired.usesSdmmc());
    TEST_ASSERT_EQUAL_INT(0, halfWired.pullUpPins(out, SdPins::kMaxPullUpPins));

    // 渡し先が無い場合も落ちない
    TEST_ASSERT_EQUAL_INT(0, kPaperMono.pullUpPins(nullptr, SdPins::kMaxPullUpPins));
    TEST_ASSERT_EQUAL_INT(0, kPaperMono.pullUpPins(out, 0));
}

int runUnityTests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_only_paper_mono_uses_sdmmc);
    RUN_TEST(test_all_devices_are_valid);
    RUN_TEST(test_pull_ups_are_only_for_sdmmc);
    RUN_TEST(test_paper_mono_pull_ups_are_the_declared_data_lines);
    RUN_TEST(test_never_touches_undeclared_pins);
    RUN_TEST(test_data_lines_need_not_be_consecutive);
    RUN_TEST(test_incomplete_wiring_is_rejected);
    return UNITY_END();
}

int main(void)
{
    return runUnityTests();
}
