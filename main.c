#include <stdio.h>
#include <stdbool.h>
#include <pigpio.h>
#include <pthread.h>

#include </usr/include/libusb-1.0/libusb.h>

#define VENDOR_ID 0x10ce
#define PRODUCT_ID 0xeb93
#define ENDPOINT_ADDR 129

#define OFFSET_BUTTON_CODE_1 2
#define OFFSET_BUTTON_CODE_2 3
#define OFFSET_FEED 4
#define OFFSET_AXIS 5
#define OFFSET_WHEEL 6

#define GPIO_MPG_A 5
#define GPIO_MPG_B 6

#define GPIO_ESTOP 26

#define GPIO_RESOLUTION_1 17
#define GPIO_RESOLUTION_2 27
#define GPIO_RESOLUTION_3 22

#define GPIO_X_AXIS_SELECT 25
#define GPIO_Y_AXIS_SELECT 8
#define GPIO_Z_AXIS_SELECT 7
#define GPIO_A_AXIS_SELECT 1

#define GPIO_CYCLE_STOP 23
#define GPIO_CYCLE_START 24

#define GPIO_PARKING 16
#define GPIO_HOME 12

#define GPIO_CONTINUOUS_MODE 13
#define GPIO_HOME_MACHINE_INPUT 18

typedef struct libusb_device_handle libusb_device_handle;

enum Axis
{
    OFF = 0x06,
    X = 0x11,
    Y,
    Z,
    A
};

enum Feed
{
    MOVE_UNIT_2 = 0x0d,
    MOVE_UNIT_5,
    MOVE_UNIT_10,
    MOVE_UNIT_30,
    MOVE_UNIT_60 = 0x1a,
    MOVE_UNIT_100,
    MOVE_UNIT_LEAD
};

enum ButtonCode1
{
    NO_BUTTON_1 = 0x00,
    RESET = 0x01,
    STOP,
    START_PAUSE,
    FEED_PLUS,
    FEED_MINUS,
    SPINDLE_PLUS,
    SPINDLE_MINUS,
    MACHINE_HOME,
    SAFE_Z,
    WORK_HOME,
    SPINDLE_TOGGLE,
    FN,
    PROBE_Z,
    CONTINUOUS,
    STEP,
    MACRO_10,
};

enum ButtonCode2
{
    NO_BUTTON_2 = 0x00,
    MACRO_1 = 0x04,
    MACRO_2,
    MACRO_3,
    MACRO_4,
    MACRO_5,
    MACRO_6,
    MACRO_7,
    MACRO_8,
    MACRO_9 = 0x0d,
};

enum JogMode
{
    CONTINUOUS_MODE,
    STEP_MODE
};

struct Readout
{
    enum ButtonCode1 buttonCode1;
    enum ButtonCode2 buttonCode2;
    enum Feed feed;
    enum Axis axis;
    int wheel;
};

struct State
{
    enum JogMode jogMode;
    enum Feed feed;
    enum Axis axis;
    enum ButtonCode1 buttonCode1;
    enum ButtonCode2 buttonCode2;
    bool feedHold;
    bool eStopPressed;
    int wheel
};

void *encoderThread(void *vargp)
{
    struct State *state = (struct State *)vargp;
    int lastA = 0;
    int lastB = 0;

    while (1)
    {
        int direction = state->wheel <= 128 ? 1 : 0;
        int frequency = direction == 1 ? state->wheel : 255 - state->wheel;

        // Frequency too small like this, multiplying
        int factor = 100.0;

        frequency = frequency * factor;

        if (frequency == 0)
        {
            lastA = 0;
            lastB = 0;
            gpioWrite(GPIO_MPG_A, 0);
            gpioWrite(GPIO_MPG_B, 0);
            continue;
        }

        if (direction == 1)
        {
            if (lastA == 0 && lastB == 0)
            {
                lastA = 1;
                gpioWrite(GPIO_MPG_A, 1);
            }
            else if (lastA == 1 && lastB == 0)
            {
                lastB = 1;
                gpioWrite(GPIO_MPG_B, 1);
            }
            else if (lastA == 1 && lastB == 1)
            {
                lastA = 0;
                gpioWrite(GPIO_MPG_A, 0);
            }
            else if (lastA == 0 && lastB == 1)
            {
                lastB = 0;
                gpioWrite(GPIO_MPG_B, 0);
            }
        }
        else
        {
            if (lastA == 0 && lastB == 0)
            {
                lastB = 1;
                gpioWrite(GPIO_MPG_B, 1);
            }
            else if (lastA == 0 && lastB == 1)
            {
                lastA = 1;
                gpioWrite(GPIO_MPG_A, 1);
            }
            else if (lastA == 1 && lastB == 1)
            {
                lastB = 0;
                gpioWrite(GPIO_MPG_B, 0);
            }
            else if (lastA == 1 && lastB == 0)
            {
                lastA = 0;
                gpioWrite(GPIO_MPG_A, 0);
            }
        }
        time_sleep(1.0 / (double)frequency / 4.0);
    }
}

int main()
{
    // Initialize pigpio
    if (gpioInitialise() < 0)
    {
        printf("Error while initializing pigpio\n");
        return -1;
    }

    // Set GPIO modes to output
    setGpioModes();

    ssize_t cnt;

    // Initialize libusb
    int initReturn = libusb_init(NULL);

    if (initReturn < 0)
    {
        printf("Error while initializing libusb\n");
        return -1;
    }

    // Find device
    libusb_device_handle *dev_handle = NULL;

    struct State state;
    state.jogMode = STEP_MODE;
    state.feed = MOVE_UNIT_5;
    state.axis = OFF;
    state.buttonCode1 = NO_BUTTON_1;
    state.buttonCode2 = NO_BUTTON_2;
    state.feedHold = false;
    state.eStopPressed = false;
    state.wheel = 0;

    pthread_t tid;

    pthread_create(&tid, NULL, encoderThread, (void *)&state);

    while (1)
    {
        dev_handle = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);

        if (dev_handle == NULL)
        {
            printf("Cannot find device\n");
            sleep(1);
            continue;
        }

        libusb_detach_kernel_driver(dev_handle, 0);

        initReturn = libusb_set_configuration(dev_handle, 1);

        if (initReturn < 0)
        {
            printf("Error while setting configuration\n");
            sleep(1);
            continue;
        }

        // Claim interface
        int claimReturn = libusb_claim_interface(dev_handle, 0);

        if (claimReturn < 0)
        {
            printf("Error while claiming interface\n");
            sleep(1);
            continue;
        }

        unsigned char data[1024] = "\0";
        int size;

        while (1)
        {
            int ret = libusb_bulk_transfer(dev_handle, ENDPOINT_ADDR, data, 0x0008, &size, 1000);
            if (ret == LIBUSB_ERROR_NO_DEVICE)
            {
                printf("Device disconnected\n");
                break;
            }

            readData(&data, &state);
            outputStateToGPIO(&state);

            ret = writeData(dev_handle, &state);
            if (ret == LIBUSB_ERROR_NO_DEVICE)
            {
                printf("Device disconnected\n");
                break;
            }
        }

        libusb_release_interface(dev_handle, 0);
        libusb_close(dev_handle);
    }

    pthread_exit(NULL);
    libusb_exit(NULL);

    return 0;
}

void setGpioModes()
{
    gpioSetMode(GPIO_MPG_A, PI_OUTPUT);
    gpioSetMode(GPIO_MPG_B, PI_OUTPUT);
    gpioSetMode(GPIO_ESTOP, PI_OUTPUT);
    gpioSetMode(GPIO_RESOLUTION_1, PI_OUTPUT);
    gpioSetMode(GPIO_RESOLUTION_2, PI_OUTPUT);
    gpioSetMode(GPIO_RESOLUTION_3, PI_OUTPUT);
    gpioSetMode(GPIO_X_AXIS_SELECT, PI_OUTPUT);
    gpioSetMode(GPIO_Y_AXIS_SELECT, PI_OUTPUT);
    gpioSetMode(GPIO_Z_AXIS_SELECT, PI_OUTPUT);
    gpioSetMode(GPIO_A_AXIS_SELECT, PI_OUTPUT);
    gpioSetMode(GPIO_CYCLE_STOP, PI_OUTPUT);
    gpioSetMode(GPIO_CYCLE_START, PI_OUTPUT);
    gpioSetMode(GPIO_PARKING, PI_OUTPUT);
    gpioSetMode(GPIO_HOME, PI_OUTPUT);
    gpioSetMode(GPIO_CONTINUOUS_MODE, PI_OUTPUT);
    gpioSetMode(GPIO_HOME_MACHINE_INPUT, PI_OUTPUT);
}

void outputStateToGPIO(const struct State *state)
{
    if (state->eStopPressed)
    {
        gpioWrite(GPIO_ESTOP, 0);
    }
    else
    {
        gpioWrite(GPIO_ESTOP, 1);
    }

    switch (state->axis)
    {
    case X:
        gpioWrite(GPIO_X_AXIS_SELECT, 1);
        gpioWrite(GPIO_Y_AXIS_SELECT, 0);
        gpioWrite(GPIO_Z_AXIS_SELECT, 0);
        gpioWrite(GPIO_A_AXIS_SELECT, 0);
        break;
    case Y:
        gpioWrite(GPIO_X_AXIS_SELECT, 0);
        gpioWrite(GPIO_Y_AXIS_SELECT, 1);
        gpioWrite(GPIO_Z_AXIS_SELECT, 0);
        gpioWrite(GPIO_A_AXIS_SELECT, 0);
        break;
    case Z:
        gpioWrite(GPIO_X_AXIS_SELECT, 0);
        gpioWrite(GPIO_Y_AXIS_SELECT, 0);
        gpioWrite(GPIO_Z_AXIS_SELECT, 1);
        gpioWrite(GPIO_A_AXIS_SELECT, 0);
        break;
    case A:
        gpioWrite(GPIO_X_AXIS_SELECT, 0);
        gpioWrite(GPIO_Y_AXIS_SELECT, 0);
        gpioWrite(GPIO_Z_AXIS_SELECT, 0);
        gpioWrite(GPIO_A_AXIS_SELECT, 1);
        break;
    default:
        gpioWrite(GPIO_X_AXIS_SELECT, 0);
        gpioWrite(GPIO_Y_AXIS_SELECT, 0);
        gpioWrite(GPIO_Z_AXIS_SELECT, 0);
        gpioWrite(GPIO_A_AXIS_SELECT, 0);
        break;
    }

    switch (state->jogMode)
    {
    case CONTINUOUS_MODE:
        gpioWrite(GPIO_CONTINUOUS_MODE, 1);
        break;
    case STEP_MODE:
        gpioWrite(GPIO_CONTINUOUS_MODE, 0);
        break;
    default:
        gpioWrite(GPIO_CONTINUOUS_MODE, 0);
        break;
    }

    switch (state->feed)
    {
    case MOVE_UNIT_5:
        gpioWrite(GPIO_RESOLUTION_1, 1);
        gpioWrite(GPIO_RESOLUTION_2, 0);
        gpioWrite(GPIO_RESOLUTION_3, 0);
        break;
    case MOVE_UNIT_10:
        gpioWrite(GPIO_RESOLUTION_1, 0);
        gpioWrite(GPIO_RESOLUTION_2, 1);
        gpioWrite(GPIO_RESOLUTION_3, 0);
        break;
    case MOVE_UNIT_30:
        gpioWrite(GPIO_RESOLUTION_1, 0);
        gpioWrite(GPIO_RESOLUTION_2, 0);
        gpioWrite(GPIO_RESOLUTION_3, 1);
        break;
    default:
        gpioWrite(GPIO_RESOLUTION_1, 0);
        gpioWrite(GPIO_RESOLUTION_2, 0);
        gpioWrite(GPIO_RESOLUTION_3, 0);
        break;
    }

    switch (state->buttonCode1)
    {
    case START_PAUSE:
        if (state->feedHold)
        {
            gpioWrite(GPIO_CYCLE_STOP, 1);
            gpioWrite(GPIO_CYCLE_START, 0);
            gpioWrite(GPIO_PARKING, 0);
            gpioWrite(GPIO_HOME, 0);
            gpioWrite(GPIO_HOME_MACHINE_INPUT, 0);
        }
        else
        {
            gpioWrite(GPIO_CYCLE_STOP, 0);
            gpioWrite(GPIO_CYCLE_START, 1);
            gpioWrite(GPIO_PARKING, 0);
            gpioWrite(GPIO_HOME, 0);
            gpioWrite(GPIO_HOME_MACHINE_INPUT, 0);
        }
        break;
    case WORK_HOME:
        gpioWrite(GPIO_CYCLE_STOP, 0);
        gpioWrite(GPIO_CYCLE_START, 0);
        gpioWrite(GPIO_PARKING, 1);
        gpioWrite(GPIO_HOME, 0);
        gpioWrite(GPIO_HOME_MACHINE_INPUT, 0);
        break;
    case MACHINE_HOME:
        gpioWrite(GPIO_CYCLE_STOP, 0);
        gpioWrite(GPIO_CYCLE_START, 0);
        gpioWrite(GPIO_PARKING, 0);
        gpioWrite(GPIO_HOME, 1);
        gpioWrite(GPIO_HOME_MACHINE_INPUT, 0);
        break;
    case MACRO_10:
        gpioWrite(GPIO_CYCLE_STOP, 0);
        gpioWrite(GPIO_CYCLE_START, 0);
        gpioWrite(GPIO_PARKING, 0);
        gpioWrite(GPIO_HOME, 0);
        gpioWrite(GPIO_HOME_MACHINE_INPUT, 1);
        break;
    case NO_BUTTON_1:
        gpioWrite(GPIO_CYCLE_STOP, 0);
        gpioWrite(GPIO_CYCLE_START, 0);
        gpioWrite(GPIO_PARKING, 0);
        gpioWrite(GPIO_HOME, 0);
        gpioWrite(GPIO_HOME_MACHINE_INPUT, 0);
    }
}

void setBit(unsigned char *buffer, int byte, int bit, bool value)
{
    if (value)
    {
        buffer[byte] |= 1 << bit;
    }
    else
    {
        buffer[byte] &= ~(1 << bit);
    }
}

int writeData(libusb_device_handle *device_handle, const struct State *state)
{
    unsigned char buffer[6 * 7] = {0x00};

    buffer[0] = 0xfe;
    buffer[1] = 0xfd;
    buffer[2] = 0x0c;
    buffer[3] = 0x00;

    setBit(buffer, 3, 0, state->jogMode == STEP);
    setBit(buffer, 3, 6, state->jogMode == CONTINUOUS);
    setBit(buffer, 3, 7, true);

    return libusb_control_transfer(device_handle, 0x09, 0x0200, 0x0306, 0x00, &buffer, 0x0008, 1000);
}

void readData(unsigned char *data, struct State *state)
{
    enum ButtonCode1 buttonCode1 = data[OFFSET_BUTTON_CODE_1];
    enum ButtonCode2 buttonCode2 = data[OFFSET_BUTTON_CODE_2];
    enum Feed feed = data[OFFSET_FEED];
    enum Axis axis = data[OFFSET_AXIS];
    int wheel = data[OFFSET_WHEEL];

    switch (buttonCode1)
    {
    case RESET:
        printf("RESET\n");
        break;
    case STOP:
        if (state->buttonCode1 == NO_BUTTON_1)
        {
            if (state->eStopPressed)
            {
                state->eStopPressed = false;
                printf("E_STOP_RELEASED\n");
            }
            else
            {
                state->eStopPressed = true;
                printf("E_STOP_PRESSED\n");
            }
        }
        break;
    case START_PAUSE:
        if (state->buttonCode1 == NO_BUTTON_1)
        {
            if (state->feedHold)
            {
                state->feedHold = false;
                printf("RESUME\n");
            }
            else
            {
                state->feedHold = true;
                printf("FEED_HOLD\n");
            }
        }
        break;
    case FEED_PLUS:
        printf("FEED_PLUS\n");
        break;
    case FEED_MINUS:
        printf("FEED_MINUS\n");
        break;
    case SPINDLE_PLUS:
        printf("SPINDLE_PLUS\n");
        break;
    case SPINDLE_MINUS:
        printf("SPINDLE_MINUS\n");
        break;
    case MACHINE_HOME:
        printf("MACHINE_HOME\n");
        break;
    case SAFE_Z:
        printf("SAFE_Z\n");
        break;
    case WORK_HOME:
        printf("WORK_HOME\n");
        break;
    case SPINDLE_TOGGLE:
        printf("SPINDLE_TOGGLE\n");
        break;
    case FN:
        printf("FN\n");
        break;
    case PROBE_Z:
        printf("PROBE_Z\n");
        break;
    case CONTINUOUS:
        state->jogMode = CONTINUOUS_MODE;
        break;
    case STEP:
        state->jogMode = STEP_MODE;
        break;
    case MACRO_10:
        printf("MACRO_10\n");
        break;
    }

    bool isFn = buttonCode1 == FN;

    if (isFn)
    {
        switch (buttonCode2)
        {
        case MACRO_1:
            printf("MACRO_1\n");
            break;
        case MACRO_2:
            printf("MACRO_2\n");
            break;
        case MACRO_3:
            printf("MACRO_3\n");
            break;
        case MACRO_4:
            printf("MACRO_4\n");
            break;
        case MACRO_5:
            printf("MACRO_5\n");
            break;
        case MACRO_6:
            printf("MACRO_6\n");
            break;
        case MACRO_7:
            printf("MACRO_7\n");
            break;
        case MACRO_8:
            printf("MACRO_8\n");
            break;
        case MACRO_9:
            printf("MACRO_9\n");
            break;
        }
    }

    state->feed = feed;
    state->axis = axis;
    state->buttonCode1 = buttonCode1;
    state->buttonCode2 = buttonCode2;
    state->wheel = wheel;

    printf("Wheel: %d\n", wheel);
}