#include <unity.h>
#include <Rect/Rect.h>

// Unit Testing / Testing Frameworks / Unity
// see: https://docs.platformio.org/en/latest/advanced/unit-testing/frameworks/unity.html

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

void test_rect_contains()
{
    Rect rect = Rect();
    rect.x = 10;
    rect.y = 10;
    rect.width = 100;
    rect.height = 50;

    TEST_ASSERT_TRUE(rect.contains(20, 30));
    TEST_ASSERT_FALSE(rect.contains(200, 300));
}

int runUnityTests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rect_contains);
    return UNITY_END();
}

/**
 * For native dev-platform or for some embedded frameworks
 */
int main(void)
{
    return runUnityTests();
}
