#include <unity.h>
#include <ImageRotation.h>

void setUp(void) {}
void tearDown(void) {}

namespace
{
    // 画面（回転していない状態）
    const int kPaperS3Width = 540;
    const int kPaperS3Height = 960;
    const int kPaperColorWidth = 400;
    const int kPaperColorHeight = 600;

    // 画像を回さない機種と、フックのぶん 180 度回す M5PaperS3
    const int kBaseUp = 0;
    const int kBaseDown = 2;

    // スマホのカメラは縦に構えても横長のまま保存し、向きは EXIF で伝える
    const int kCameraWidth = 4032;
    const int kCameraHeight = 3024;
    const int kOrientationNone = 1;
    const int kOrientationRotate90 = 6;
}

void test_portrait_photo_is_not_rotated_to_fit()
{
    // 縦に構えて撮った写真。EXIF のぶんだけ回れば、画面と向きが揃うので
    // それ以上は回さない。
    TEST_ASSERT_EQUAL_INT(1, ImageFile::displayRotation(kOrientationRotate90, kCameraWidth, kCameraHeight,
                                                        kBaseUp, kPaperColorWidth, kPaperColorHeight));

    TEST_ASSERT_EQUAL_INT(1, ImageFile::displayRotation(kOrientationRotate90, kCameraWidth, kCameraHeight,
                                                        kBaseUp, kPaperS3Width, kPaperS3Height));
}

void test_portrait_photo_on_flipped_device()
{
    // M5PaperS3 はフックのぶん 180 度回っている。EXIF の 90 度を足して 3。
    // 向きは揃っているので、そこから先は回さない。
    TEST_ASSERT_EQUAL_INT(3, ImageFile::displayRotation(kOrientationRotate90, kCameraWidth, kCameraHeight,
                                                        kBaseDown, kPaperS3Width, kPaperS3Height));
}

void test_landscape_photo_is_rotated_counter_clockwise()
{
    // 横に構えて撮った写真は EXIF で回らないので、画面に合わせて反時計回りに回す
    TEST_ASSERT_EQUAL_INT(3, ImageFile::displayRotation(kOrientationNone, kCameraWidth, kCameraHeight,
                                                        kBaseUp, kPaperColorWidth, kPaperColorHeight));

    // 180 度回っている機種では 2 + 3 = 5 → 1
    TEST_ASSERT_EQUAL_INT(1, ImageFile::displayRotation(kOrientationNone, kCameraWidth, kCameraHeight,
                                                        kBaseDown, kPaperS3Width, kPaperS3Height));
}

void test_screen_sized_image_keeps_base_rotation()
{
    // 画面ぴったりに作った画像は、これまでどおり機種の既定の向きのまま
    TEST_ASSERT_EQUAL_INT(kBaseDown, ImageFile::displayRotation(kOrientationNone, kPaperS3Width, kPaperS3Height,
                                                                kBaseDown, kPaperS3Width, kPaperS3Height));

    TEST_ASSERT_EQUAL_INT(kBaseUp, ImageFile::displayRotation(kOrientationNone, kPaperColorWidth, kPaperColorHeight,
                                                              kBaseUp, kPaperColorWidth, kPaperColorHeight));
}

void test_unknown_size_falls_back_to_exif_only()
{
    // 寸法が読めなければ EXIF の向きだけで決める
    TEST_ASSERT_EQUAL_INT(1, ImageFile::displayRotation(kOrientationRotate90, 0, 0,
                                                        kBaseUp, kPaperColorWidth, kPaperColorHeight));

    TEST_ASSERT_EQUAL_INT(kBaseDown, ImageFile::displayRotation(kOrientationNone, 0, 0,
                                                                kBaseDown, kPaperS3Width, kPaperS3Height));
}

void test_fit_steps_rotates_when_orientation_differs()
{
    // 横長の写真を縦長の画面に出すときは反時計回りに 90 度回す
    TEST_ASSERT_EQUAL_INT(3, ImageFile::orientationFitSteps(4032, 3024, 540, 960));

    // 縦長の写真を縦長の画面に出すときはそのまま
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(3024, 4032, 540, 960));

    // 正方形は縦長の画面に対して回さない
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(1000, 1000, 540, 960));

    // 寸法が取れなかった場合は回さない
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(0, 0, 540, 960));
}

int runUnityTests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_portrait_photo_is_not_rotated_to_fit);
    RUN_TEST(test_portrait_photo_on_flipped_device);
    RUN_TEST(test_landscape_photo_is_rotated_counter_clockwise);
    RUN_TEST(test_screen_sized_image_keeps_base_rotation);
    RUN_TEST(test_unknown_size_falls_back_to_exif_only);
    RUN_TEST(test_fit_steps_rotates_when_orientation_differs);
    return UNITY_END();
}

int main(void)
{
    return runUnityTests();
}
