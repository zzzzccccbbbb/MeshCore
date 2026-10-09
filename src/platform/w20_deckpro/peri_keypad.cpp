// Keypad driver: supports TCA8418 (I2C) or GPIO-based keys (BOOT/LED only)
#include <Arduino.h>
#include "utilities.h"
#include "peripheral.h"

#if USE_TCA8418_KEYPAD
#include <Adafruit_TCA8418.h>

#define KEYPAD_ROWS 4
#define KEYPAD_COLS 10
#define KEYPAD_PRESS_VAL_MIN   129
#define KEYPAD_PRESS_VAL_MAX   163
#define KEYPAD_RELEASE_VAL_MIN 1
#define KEYPAD_RELEASE_VAL_MAX 35

const char keymap[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '0'},
    {'2', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '$', 'E'},
    {' ', ' ', ' ', ' ', ' ', '-', '*', 'S', '0', 'U'},
};

Adafruit_TCA8418 keypad; 
keypad_cb keypad_listener = nullptr;
char keypad_curr_val = ' ';
int keypad_state = KEYPAD_RELEASE;
bool keypad_update = false;

bool keypad_init(int address)
{
    if(!i2cIsInit(0)){
        Wire.begin(BOARD_KEYBOARD_SDA, BOARD_KEYBOARD_SCL);
        Wire.beginTransmission(address);
        Wire.endTransmission(true);
    }

    if (!keypad.begin(address, &Wire)) {
        // Serial.println("keypad not found, check wiring & pullups!");
        log_e("keypad not found, check wiring & pullups!");
        return false;
    }

    // configure the size of the keypad matrix.
    // all other pins will be inputs
    keypad.matrix(KEYPAD_ROWS, KEYPAD_COLS);

    // flush the internal buffer
    keypad.flush();

    return true;
}

int keypad_get_val(char *c)
{
    if(c){
        *c = keypad_curr_val;
    }
    return keypad_update;
} 

void keypad_set_flag(void)
{
    keypad_update = false;
}

void keypad_loop(void)
{
    char c = -1;
    int state = -1;
    int row, col;
    int k = keypad.getEvent();
    int v = keypad.available();

    if(k >=KEYPAD_RELEASE_VAL_MIN && k <= KEYPAD_RELEASE_VAL_MAX){ // release event
        k = k - KEYPAD_RELEASE_VAL_MIN;
        state = KEYPAD_RELEASE;
    }   

    if(k >=KEYPAD_PRESS_VAL_MIN && k <= KEYPAD_PRESS_VAL_MAX){ // press event
        k = k - KEYPAD_PRESS_VAL_MIN;
        state = KEYPAD_PRESS;
    }

    if(state != -1){
        row = k / KEYPAD_COLS;
        col = (KEYPAD_COLS-1) - k % KEYPAD_COLS;
        c = keymap[row][col];
        Serial.printf("k=%d, v=%d, press:%d, %d, %c\n", k, v, row, col, c);
        // if(keypad_listener)
        //     keypad_listener(state, c);
        
        keypad_curr_val = c;
        keypad_state = state;
        keypad_update = true;
    }
}

void keypad_regetser_cb(keypad_cb cb)
{
    keypad_listener = cb;
}

#else // GPIO keypad shim

// Simple GPIO-based keypad: BOOT (GPIO0), LED_KEY (GPIO48) — three physical keys are RST+BOOT+LED, no cap touch
// Exposes same API: keypad_init/get_val/loop/register_cb

static keypad_cb keypad_listener = nullptr;
static char keypad_curr_val = ' ';
static int keypad_state = KEYPAD_RELEASE;
static bool keypad_update = false;
static int last_boot = 0, last_led = 0;

static inline int read_boot() {
    if (PIN_KEY_BOOT < 0) return 0;
    pinMode(PIN_KEY_BOOT, INPUT);
    return digitalRead(PIN_KEY_BOOT) == LOW ? 1 : 0;
}
static inline int read_led() {
    if (PIN_KEY_LED < 0) return 0;
    pinMode(PIN_KEY_LED, INPUT);
    return digitalRead(PIN_KEY_LED) == LOW ? 1 : 0;
}

bool keypad_init(int address)
{
    (void)address;
    last_boot = read_boot();
    last_led  = read_led();
    keypad_update = false;
    keypad_curr_val = ' ';
    keypad_state = KEYPAD_RELEASE;
    return true;
}

int keypad_get_val(char *c)
{
    // Edge-detect: on press, report one event per key; priority: BOOT -> LED
    int b = read_boot();
    int l = read_led();
    if (b && !last_boot) {
        keypad_curr_val = 'B';
        keypad_state = KEYPAD_PRESS;
        keypad_update = true;
    } else if (l && !last_led) {
        keypad_curr_val = 'L';
        keypad_state = KEYPAD_PRESS;
        keypad_update = true;
    }
    last_boot = b; last_led = l;
    if (c) *c = keypad_curr_val;
    return keypad_update;
}

void keypad_set_flag(void)
{
    keypad_update = false;
}

void keypad_loop(void)
{
    // No periodic work needed; polling is done in keypad_get_val()
}

void keypad_regetser_cb(keypad_cb cb)
{
    keypad_listener = cb;
}

#endif // USE_TCA8418_KEYPAD