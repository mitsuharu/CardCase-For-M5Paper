#include <unity.h>
#include <DeviceProfile.h>

// 機種ごとのプロファイルの検証。
// 実機がなくても機種差分の取り違えに気付けるようにしておく。

void setUp(void) {}
void tearDown(void) {}

void test_image_rotation_is_flipped_only_on_paper_s3()
{
    // M5PaperS3 は筐体上部のフックに掛けると上下が逆になるので反転させる
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperS3).imageRotation == DeviceRotation::Down);

    // フックのない機種はそのまま
    TEST_ASSERT_TRUE(profileFor(DeviceKind::M5Paper).imageRotation == DeviceRotation::Up);
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperColor).imageRotation == DeviceRotation::Up);
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperMono).imageRotation == DeviceRotation::Up);
}

void test_paper_color_has_no_touch_but_has_buttons()
{
    DeviceProfile profile = profileFor(DeviceKind::PaperColor);

    // タッチパネルを積んでいないのでボタンだけが操作手段になる
    TEST_ASSERT_FALSE(profile.hasTouch);
    TEST_ASSERT_TRUE(profile.hasButtons());
    TEST_ASSERT_TRUE(profile.isOperable());

    // 上下のボタンが BtnA / BtnB に隣り合い、決定は離れた BtnC にある
    TEST_ASSERT_TRUE(profile.buttonA == ButtonRole::Prev);
    TEST_ASSERT_TRUE(profile.buttonB == ButtonRole::Next);
    TEST_ASSERT_TRUE(profile.buttonC == ButtonRole::Select);
}

void test_m5paper_follows_rotary_switch_order()
{
    DeviceProfile profile = profileFor(DeviceKind::M5Paper);

    // ロータリスイッチの 上 / 押込 / 下 がそのまま BtnA / BtnB / BtnC に並ぶ
    TEST_ASSERT_TRUE(profile.buttonA == ButtonRole::Prev);
    TEST_ASSERT_TRUE(profile.buttonB == ButtonRole::Select);
    TEST_ASSERT_TRUE(profile.buttonC == ButtonRole::Next);
}

void test_paper_mono_uses_two_buttons()
{
    DeviceProfile profile = profileFor(DeviceKind::PaperMono);

    // ボタンが 2 つしかないので送りと決定だけに割り当てる
    TEST_ASSERT_TRUE(profile.buttonA == ButtonRole::Next);
    TEST_ASSERT_TRUE(profile.buttonB == ButtonRole::Select);
    TEST_ASSERT_TRUE(profile.buttonC == ButtonRole::None);
}

void test_paper_s3_has_touch_but_no_buttons()
{
    DeviceProfile profile = profileFor(DeviceKind::PaperS3);

    // 物理ボタンは電源ボタンだけなので一覧の操作はタッチに頼る
    TEST_ASSERT_TRUE(profile.hasTouch);
    TEST_ASSERT_FALSE(profile.hasButtons());
    TEST_ASSERT_TRUE(profile.isOperable());
}

void test_display_sizes()
{
    TEST_ASSERT_EQUAL_INT(540, profileFor(DeviceKind::M5Paper).width);
    TEST_ASSERT_EQUAL_INT(960, profileFor(DeviceKind::M5Paper).height);

    TEST_ASSERT_EQUAL_INT(540, profileFor(DeviceKind::PaperS3).width);
    TEST_ASSERT_EQUAL_INT(960, profileFor(DeviceKind::PaperS3).height);

    TEST_ASSERT_EQUAL_INT(400, profileFor(DeviceKind::PaperColor).width);
    TEST_ASSERT_EQUAL_INT(600, profileFor(DeviceKind::PaperColor).height);

    TEST_ASSERT_EQUAL_INT(480, profileFor(DeviceKind::PaperMono).width);
    TEST_ASSERT_EQUAL_INT(800, profileFor(DeviceKind::PaperMono).height);
}

void test_palette_and_optional_features()
{
    // カラー表示は M5PaperColor だけ
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperColor).isColor());
    TEST_ASSERT_FALSE(profileFor(DeviceKind::PaperMono).isColor());
    TEST_ASSERT_FALSE(profileFor(DeviceKind::PaperS3).isColor());

    // フロントライトと NFC は M5PaperMono だけ
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperMono).hasFrontlight);
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperMono).hasNfc);
    TEST_ASSERT_FALSE(profileFor(DeviceKind::PaperColor).hasNfc);
    TEST_ASSERT_FALSE(profileFor(DeviceKind::M5Paper).hasFrontlight);
}

void test_paper_mono_avoids_the_flashing_refresh()
{
    // 4 階調しか出ない機種では、反転を繰り返す波形に見合う画質差が出ない
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperMono).imageRefresh == RefreshMode::Fast);

    // 16 階調の機種は画質を優先する
    TEST_ASSERT_TRUE(profileFor(DeviceKind::M5Paper).imageRefresh == RefreshMode::Quality);
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperS3).imageRefresh == RefreshMode::Quality);
}

void test_only_paper_color_has_slow_refresh()
{
    // M5PaperColor は部分更新が無く、1 回の更新に十数秒かかる
    TEST_ASSERT_TRUE(profileFor(DeviceKind::PaperColor).slowRefresh);

    TEST_ASSERT_FALSE(profileFor(DeviceKind::M5Paper).slowRefresh);
    TEST_ASSERT_FALSE(profileFor(DeviceKind::PaperS3).slowRefresh);
    TEST_ASSERT_FALSE(profileFor(DeviceKind::PaperMono).slowRefresh);
}

void test_unknown_device_is_not_operable()
{
    DeviceProfile profile = profileFor(DeviceKind::Unknown);

    TEST_ASSERT_FALSE(profile.isOperable());
    TEST_ASSERT_EQUAL_INT(0, profile.width);
}

int runUnityTests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_image_rotation_is_flipped_only_on_paper_s3);
    RUN_TEST(test_paper_color_has_no_touch_but_has_buttons);
    RUN_TEST(test_m5paper_follows_rotary_switch_order);
    RUN_TEST(test_paper_mono_uses_two_buttons);
    RUN_TEST(test_paper_s3_has_touch_but_no_buttons);
    RUN_TEST(test_display_sizes);
    RUN_TEST(test_palette_and_optional_features);
    RUN_TEST(test_paper_mono_avoids_the_flashing_refresh);
    RUN_TEST(test_only_paper_color_has_slow_refresh);
    RUN_TEST(test_unknown_device_is_not_operable);
    return UNITY_END();
}

int main(void)
{
    return runUnityTests();
}
