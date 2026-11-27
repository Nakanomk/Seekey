#include "unity.h"

/* Exercise the production classifier without opening an input device. */
#include "../src/input.c"

static void test_touchpad_classification_excludes_direct_touch_and_pen(void)
{
    struct libevdev *dev = libevdev_new();
    struct input_absinfo axis = {.maximum = 2000, .resolution = 20};
    libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_X, &axis);
    libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_Y, &axis);
    libevdev_enable_event_code(dev, EV_KEY, BTN_TOUCH, NULL);
    TEST_ASSERT_FALSE(device_is_touchpad(dev));
    /* A clickpad need not advertise BTN_RIGHT or any relative axes. */
    libevdev_enable_property(dev, INPUT_PROP_BUTTONPAD);
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    TEST_ASSERT_TRUE(device_is_touchpad(dev));
    libevdev_enable_property(dev, INPUT_PROP_DIRECT);
    TEST_ASSERT_FALSE(device_is_touchpad(dev));
    libevdev_free(dev);

    dev = libevdev_new();
    libevdev_enable_event_code(dev, EV_ABS, ABS_X, &axis);
    libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &axis);
    libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_FINGER, NULL);
    TEST_ASSERT_TRUE(device_is_touchpad(dev));
    libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_PEN, NULL);
    TEST_ASSERT_FALSE(device_is_touchpad(dev));
    libevdev_free(dev);
}

int run_input_tests(void)
{
    UnityBegin("test_input.c");
    RUN_TEST(test_touchpad_classification_excludes_direct_touch_and_pen);
    return UnityEnd();
}
