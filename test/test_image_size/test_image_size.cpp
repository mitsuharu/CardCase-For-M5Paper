#include <unity.h>
#include <ImageSize.h>
#include <vector>

void setUp(void) {}
void tearDown(void) {}

namespace
{
    void pushU16BE(std::vector<uint8_t> &out, uint16_t value)
    {
        out.push_back(static_cast<uint8_t>(value >> 8));
        out.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    /// SOF0 だけを持つ最小の JPEG を組み立てる。leadingSegments は前に挟むダミー。
    std::vector<uint8_t> makeJpeg(int width, int height, size_t paddingBytes = 0)
    {
        std::vector<uint8_t> jpeg = {0xFF, 0xD8};

        if (paddingBytes > 0)
        {
            // EXIF のサムネイルのように大きな APP1 が先にある状況を模す
            jpeg.push_back(0xFF);
            jpeg.push_back(0xE1);
            pushU16BE(jpeg, static_cast<uint16_t>(paddingBytes + 2));
            jpeg.insert(jpeg.end(), paddingBytes, 0x00);
        }

        jpeg.push_back(0xFF);
        jpeg.push_back(0xC0); // SOF0
        pushU16BE(jpeg, 17);  // 長さ
        jpeg.push_back(8);    // 精度
        pushU16BE(jpeg, static_cast<uint16_t>(height));
        pushU16BE(jpeg, static_cast<uint16_t>(width));
        jpeg.insert(jpeg.end(), 10, 0x00); // 成分の記述（内容は問わない）

        return jpeg;
    }

    std::vector<uint8_t> makePng(int width, int height)
    {
        std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        png.insert(png.end(), {0x00, 0x00, 0x00, 0x0D});
        png.insert(png.end(), {'I', 'H', 'D', 'R'});
        for (int shift = 24; shift >= 0; shift -= 8)
        {
            png.push_back(static_cast<uint8_t>((width >> shift) & 0xFF));
        }
        for (int shift = 24; shift >= 0; shift -= 8)
        {
            png.push_back(static_cast<uint8_t>((height >> shift) & 0xFF));
        }
        return png;
    }
}

void test_reads_jpeg_size()
{
    std::vector<uint8_t> jpeg = makeJpeg(4032, 3024);

    int width = 0;
    int height = 0;
    TEST_ASSERT_TRUE(ImageFile::imageSize(jpeg.data(), jpeg.size(), &width, &height));
    TEST_ASSERT_EQUAL_INT(4032, width);
    TEST_ASSERT_EQUAL_INT(3024, height);
}

void test_reads_jpeg_size_after_large_exif()
{
    // EXIF にサムネイルが入っていても SOF まで辿り着けること
    std::vector<uint8_t> jpeg = makeJpeg(1200, 1600, 20000);

    int width = 0;
    int height = 0;
    TEST_ASSERT_TRUE(ImageFile::imageSize(jpeg.data(), jpeg.size(), &width, &height));
    TEST_ASSERT_EQUAL_INT(1200, width);
    TEST_ASSERT_EQUAL_INT(1600, height);
}

void test_reads_png_size()
{
    std::vector<uint8_t> png = makePng(540, 960);

    int width = 0;
    int height = 0;
    TEST_ASSERT_TRUE(ImageFile::imageSize(png.data(), png.size(), &width, &height));
    TEST_ASSERT_EQUAL_INT(540, width);
    TEST_ASSERT_EQUAL_INT(960, height);
}

void test_returns_false_for_broken_input()
{
    int width = -1;
    int height = -1;

    TEST_ASSERT_FALSE(ImageFile::imageSize(nullptr, 0, &width, &height));

    const uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    TEST_ASSERT_FALSE(ImageFile::imageSize(garbage, sizeof(garbage), &width, &height));

    // 途中で切れていても落ちないこと
    std::vector<uint8_t> jpeg = makeJpeg(640, 480);
    for (size_t size = 0; size < jpeg.size(); size++)
    {
        ImageFile::imageSize(jpeg.data(), size, &width, &height);
    }
}

void test_fit_steps_rotates_when_orientation_differs()
{
    // 横長の写真を縦長の画面に出すときは 90 度回す
    TEST_ASSERT_EQUAL_INT(1, ImageFile::orientationFitSteps(4032, 3024, 540, 960));

    // 縦長の写真を縦長の画面に出すときはそのまま
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(3024, 4032, 540, 960));

    // 画面がすでに横向きなら、横長の写真はそのまま
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(4032, 3024, 960, 540));
    TEST_ASSERT_EQUAL_INT(1, ImageFile::orientationFitSteps(3024, 4032, 960, 540));
}

void test_fit_steps_keeps_square_and_matching_images()
{
    // 画面ぴったりの画像は動かさない
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(540, 960, 540, 960));
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(400, 600, 400, 600));

    // 正方形は縦長の画面に対して回さない
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(1000, 1000, 540, 960));

    // 寸法が取れなかった場合は回さない
    TEST_ASSERT_EQUAL_INT(0, ImageFile::orientationFitSteps(0, 0, 540, 960));
}

int runUnityTests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reads_jpeg_size);
    RUN_TEST(test_reads_jpeg_size_after_large_exif);
    RUN_TEST(test_reads_png_size);
    RUN_TEST(test_returns_false_for_broken_input);
    RUN_TEST(test_fit_steps_rotates_when_orientation_differs);
    RUN_TEST(test_fit_steps_keeps_square_and_matching_images);
    return UNITY_END();
}

int main(void)
{
    return runUnityTests();
}
