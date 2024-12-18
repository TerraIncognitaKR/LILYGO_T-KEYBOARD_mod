/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : BLE_keyboard.ino
  * @brief          : Customized Firmware for LILYGO T-Keyboard
  ******************************************************************************
  * @attention
  *
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Arduino.h"
#include "img.h"

#include <SPI.h>
#include <TFT_GC9D01N.h>
#include <BleKeyboard.h>



/* Defines -------------------------------------------------------------------*/

// Firmware Version Info
#define FW_VER                  1

// TFT-LCD
#define TFT_HIGH                40
#define TFT_WIDE                160
#define GAP                     8
#define GAP_Y                   2
// Keyboard Backlight Control
#define keyborad_BL_PIN         9

// Keyboard - Special Keys
#define KEYIDX_ALT_COL          0
#define KEYIDX_ALT_ROW          4
#define KEYIDX_LSH_COL          1
#define KEYIDX_LSH_ROW          6
#define KEYIDX_RSH_COL          2
#define KEYIDX_RSH_ROW          3
#define KEYIDX_ENTER_COL        3
#define KEYIDX_ENTER_ROW        3
#define KEYIDX_SYM_COL          0
#define KEYIDX_SYM_ROW          2
#define KEYIDX_MIC_COL          0
#define KEYIDX_MIC_ROW          6
#define KEYIDX_BKSP_COL         4
#define KEYIDX_BKSP_ROW         3



/* Macros --------------------------------------------------------------------*/

/**
 * @note    if defines, [Caps Lock] Key is [RShift]
 */
#define CAPSLOCK_MAP_RSH        1

/**
 * @note    if defines, Input Method Changes using [Alt]+[RShift]
 */
#define IME_SWHTCH_ALT          1

/**
 * @note    if defines, [SYM] key behaves in a similar way to [Caps Lock]
 */
#define SYMBOL_LOCK              1

/**
 * @note    if defines, enable local echo
 */
#define ECHO_LCD_EN 1
#define ECHO_SER_EN 1

/* Types/Class ---------------------------------------------------------------*/

/**
 * @note    LCD Connection --- defined by hardware. see "T-Keyboard.jpg"
 *          used :      TFT_DC / TFT_BL / TFT_MOSI / TFT_SCLK
 *          unused :    TFT_MISO / TFT_CS / TFT_RST
 */
TFT_GC9D01N_Class TFT_099;

// BLE Keyboard --- deviceName(max 16 char), deviceManufacture, batteryLevel
char  DeviceName[] = "TERRA-TKeyboard";
BleKeyboard bleKeyboard(DeviceName, "ESPRESSIF", 100);



/* Variables -----------------------------------------------------------------*/

/**
 * @note    Keyboard Matrix --- defined by hardware. see "T-Keyboard.jpg"
 */
byte rows[] = {0, 3, 18, 12, 11, 6, 7};
const int rowCount = sizeof(rows) / sizeof(rows[0]);
byte cols[] = {1, 4, 5, 19, 13};
const int colCount = sizeof(cols) / sizeof(cols[0]);

/**
 * @note    Current Keypressed State
 *          true : pressed / false : not pressed / @ref buttonPressed
 */
bool keys[colCount][rowCount];

/**
 * @note    Last Keypressed State
 *          true : pressed before / false : not pressed before
 */
bool lastValue[colCount][rowCount];

/**
 * @note    State of Keypress state has changed
 *          true : changed / false : not changed
 */
bool changedValue[colCount][rowCount];

/**
 * @note    Store Assigned Keyboard Characters
 */
char keyboard[colCount][rowCount];              // characters for normal
char keyboard_symbol[colCount][rowCount];       // characters for symbol

/**
 * @note    Keyboard Input State such as Alt/Caps Lock/...
 */
bool symbolSelected;                            // [Symbol] press state
bool case_locking = false;                      // Caps Lock state              toggle default [RShift]
bool alt_active = false;                        // [ALT] press state

bool keyborad_BL_state = true;                  // Keyboard Backlight state     true=OFF, false=ON
bool display_connected = true;                  // The bluetooth connection is displayed on the screen

/**
 * @note    TFT-LCD Control relative
 */
int OffsetX = 0;
uint16_t flow_i = 0;

/**
 * @note    System relative
 */
unsigned long previousMillis_1      = 0;        // Millisecond time record
unsigned long previousMillis_2      = 0;        // Millisecond time record
const long backlight_off_time       = 20000;    // Turn off the screen backlight
const long display_Wait_blue_time   = 2000;     // The screen shows waiting for bluetooth connection



/* Function prototypes -------------------------------------------------------*/
void readMatrix();
bool keyPressed(int colIndex, int rowIndex);
bool keyActive(int colIndex, int rowIndex);
bool isPrintableKey(int colIndex, int rowIndex);
void printMatrix();
void set_keyborad_BL(bool state);
void clearScreen();
void assignChar();
void displayKbdStatus();
void displayHelpMsg(void);


/* User code -----------------------------------------------------------------*/



/**
  * @brief      arduino setup function
  * @param      none
  * @return     none
  * @note       The setup() function is called when a sketch starts.
  *             Use it to initialize variables, pin modes, start using libraries, etc.
  *             The setup() function will only run once,
  *             after each powerup or reset of the Arduino board.
  */
void setup()
{
    // start debug console
    Serial.begin(115200);
    delay(500);

    // splash
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println("***************************************************************************");
    Serial.printf("\t[ %s ]\r\n", DeviceName);
    Serial.println();
    Serial.printf("\tFW Ver 0x%02X\r\n", FW_VER);
    Serial.printf("\tBuild @ %s %s\r\n", __DATE__, __TIME__);
    Serial.println();
    Serial.println();
    Serial.println("***************************************************************************");

    displayHelpMsg();

    Serial.printf(">> Config Keypad IO Pin...\r\n");

    // assign characters
    assignChar();

    // Initialize IO Pin state of "Keyboard Backlight Control"
    pinMode(keyborad_BL_PIN, OUTPUT);
    set_keyborad_BL(keyborad_BL_state); // see declare

    // Initialize BLE Keyboard
    bleKeyboard.begin();

    // Initialize IO Pin state of Keypad
    for (int x = 0; x < rowCount; x++)
    {
        Serial.printf(">> %02d", rows[x]); Serial.println(" as input");
        pinMode(rows[x], INPUT);
    }
    for (int x = 0; x < colCount; x++)
    {
        Serial.printf(">> %02d", cols[x]); Serial.println(" as input-pullup");
        pinMode(cols[x], INPUT_PULLUP);
    }

    Serial.printf(">> OK!\r\n");


    // set [Symbol] press state to false
    symbolSelected = false;

    // Initialize LCD
    Serial.printf(">> Initialize LCD...\r\n");
    TFT_099.begin();
    TFT_099.backlight(50);  // !! Do not set the screen backlight to too high, which may cause overexposure !!
    TFT_099.DispColor(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);

#if 0
    // TFT-LCD Splash Logo --- colorful LILYGO logo
    Serial.printf(">> Display Splash Screen...\r\n");
    TFT_099.DrawImage(0, 0, 40, 160, liligo_logo); delay(2000);

    // TFT-LCD Splash Logo --- Flow of the logo
    while (millis() < 6000)
    {
        for (int j = 0; j < 4; j++)
        {
            TFT_099.DrawImage(0, (160 - (flow_i + j * 55)), 40, 40, liligo_logo1);
        }

        flow_i++;

        if (flow_i == 55)
        {
            flow_i = 0;
        }
    }
#else
    TFT_099.DispStr(DeviceName, 2, 2, CYAN, BLACK);
    TFT_099.DispStr(__DATE__, 2, (GAP_Y+FONT_H+2), CYAN, BLACK);
    delay(1000);
#endif

    // end of initialization
    Serial.printf(">> Waiting Bluetooth...\r\n");
    clearScreen();
    // TFT_099.DispColor(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
    TFT_099.DispStr("Wait Bluetooth...", 2, 2, LIGHT_BLUE, BLACK);
    delay(500);
}



/**
  * @brief      arduino loop function
  * @param      none
  * @return     none
  * @note       After creating a setup() function,
  *             which initializes and sets the initial values,
  *             the loop() function does precisely what its name suggests,
  *             and loops consecutively, allowing your program to change and respond.
  *             Use it to actively control the Arduino board.
  */
void loop()
{

    // [ALT]+[B]        Change keyboard backlight status
    if (alt_active && keyPressed(3, 4))
    {
        alt_active = false;
        TFT_099.DispColor(0, 0, TFT_HIGH, TFT_WIDE, BLACK);
        keyborad_BL_state = !keyborad_BL_state;
        set_keyborad_BL(keyborad_BL_state);
        clearScreen();
    }
    // [ALT]+[I]        Up Arrow
    if (alt_active && keyPressed(4, 2))
    {
        alt_active = false;
        bleKeyboard.write(KEY_UP_ARROW);
    }
    // [ALT]+[K]        Down Arrow
    if (alt_active && keyPressed(4, 6))
    {
        alt_active = false;
        bleKeyboard.write(KEY_DOWN_ARROW);
    }
    // [ALT]+[J]        Left Arrow
    if (alt_active && keyPressed(3, 6))
    {
        alt_active = false;
        bleKeyboard.write(KEY_LEFT_ARROW);
    }
    // [ALT]+[L]        Right Arrow
    if (alt_active && keyPressed(4, 1))
    {
        alt_active = false;
        bleKeyboard.write(KEY_RIGHT_ARROW);
    }
    // [ALT]+[Space]    Tab
    if (alt_active && keyPressed(0, 5))
    {
        alt_active = false;
        bleKeyboard.write(KEY_TAB);
    }
    // [ALT]+[Z]        Prev Track
    if (alt_active && keyPressed(1, 5))
    {
        alt_active = false;
        bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
    }
    // [ALT]+[X]        Play/Pause
    if (alt_active && keyPressed(1, 4))
    {
        alt_active = false;
        bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
    }
    // [ALT]+[C]        Next Track
    if (alt_active && keyPressed(2, 5))
    {
        alt_active = false;
        bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
    }
    // [ALT]+[V]        Volume Down
    if (alt_active && keyPressed(2, 4))
    {
        alt_active = false;
        bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
    }
    // [ALT]+[N]        Volume Up
    if (alt_active && keyPressed(3, 5))
    {
        alt_active = false;
        bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
    }
    // [ALT]+[M]        Mute
    if (alt_active && keyPressed(4, 5))
    {
        alt_active = false;
        bleKeyboard.write(KEY_MEDIA_MUTE);
    }

#if 0
    // [ALT]+[Bksp]     Delete
    if (alt_active && keyPressed(4, 3))
    {
        alt_active = false;
        bleKeyboard.write(KEY_DELETE);
    }
    /***
     *  @ref    https://www.temblast.com/ref/akeyscode.htm
     *  @note   these keys are not supported by current library.
     *
     *
     ***/
#endif

#ifdef SYMBOL_LOCK
    //
    if(keyPressed(KEYIDX_SYM_COL, KEYIDX_SYM_ROW))
    {
        symbolSelected = !symbolSelected;

        (symbolSelected) ? (Serial.println("SYM Mode")) : (Serial.println("Normal Mode"));
    }
#endif

#ifdef CAPSLOCK_MAP_RSH
    // [LShift]        Toggle "Caps Lock"
    if (keyPressed(KEYIDX_LSH_COL, KEYIDX_LSH_ROW))
#else
    // [RShift]        Toggle "Caps Lock"
    if (keyPressed(KEYIDX_RSH_COL, KEYIDX_RSH_ROW))
#endif
    {
        case_locking = !case_locking;
    }

    // when BLE is connected ...
    if (bleKeyboard.isConnected())
    {
        // No keyboard input for 2 seconds. Turn off the screen backlight
        if (millis() - previousMillis_1  > backlight_off_time)
        {
            TFT_099.backlight(0);
            previousMillis_1 = millis();;
        }

        if (display_connected)
        {
            Serial.printf(">> Bluetooth Connected!\r\n");
            TFT_099.backlight(50);
            TFT_099.DispStr("Bluetooth connected", 0, 2, LIGHT_BLUE, BLACK);
            display_connected = false;
        }

        // Read     Keypress State
        readMatrix();

        // Printout Keypress State
        printMatrix();

        // Display  Special Key Status
        displayKbdStatus();


        /********* Key Action *********/

        // [ENTER]
        if (keyPressed(KEYIDX_ENTER_COL, KEYIDX_ENTER_ROW))
        {
            clearScreen();
            Serial.println();
            bleKeyboard.println();
        }

        // [Backspace]
        if (keyPressed(KEYIDX_BKSP_COL, KEYIDX_BKSP_ROW))
        {
            if (OffsetX < 8)
            {
                OffsetX = 0;                // set leftmost position
            }
            else
            {
                OffsetX = OffsetX - GAP;    // back to 1 char
            }

            TFT_099.DispColor(0, OffsetX, TFT_HIGH, TFT_WIDE, BLACK);
            bleKeyboard.press(KEY_BACKSPACE);
        }

        // [LShift] --- redirect to [RShift]
        if (keyPressed(KEYIDX_LSH_COL, KEYIDX_LSH_ROW))
        {
            bleKeyboard.press(KEY_RIGHT_SHIFT);
        }

        // Change Input Method
        /**
         *  @note   Switch Input Method
         *          press [Alt]+[LShift] to trigger [Ctrl]+[Shift] (common)
         *          but it is have a issue. @ref https://blog.naver.com/ssd325/222954601098
         *          _m_ change key combination to [Alt]+[RShift]
         */
#ifdef IME_SWHTCH_ALT
        if (keyActive(KEYIDX_ALT_COL, KEYIDX_ALT_ROW) && keyPressed(KEYIDX_RSH_COL, KEYIDX_RSH_ROW))
        {
            bleKeyboard.press(KEY_RIGHT_ALT);
        }
#else
        if (keyActive(0, 4) && keyPressed(1, 6))
        {
            bleKeyboard.press(KEY_RIGHT_CTRL);
            bleKeyboard.press(KEY_RIGHT_SHIFT);
        }
#endif

        bleKeyboard.releaseAll();

    }

    // when BLE is NOT connected ...
    else
    {
        if (millis() - previousMillis_2 > display_Wait_blue_time )
        {
            Serial.printf(">> Waiting Bluetooth...\r\n");
            TFT_099.DispColor(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
            TFT_099.DispStr("Wait bluetooth ......", 0, 2, LIGHT_BLUE, BLACK);
            display_connected = true;
            previousMillis_2 = millis();
        }
    }

}



/**
  * @brief      Keyboard Backlight Control
  * @param      state   true : ON / false : OFF
  * @return     none
  * @note       digitalWrite(keyborad_BL_PIN=9, state)
  *             press alt+B to toggle (default)
  */
void set_keyborad_BL(bool state)
{
    digitalWrite(keyborad_BL_PIN, state);
}



/**
  * @brief      Clear LCD Screen & Reset X position
  * @param      none
  * @return     none
  * @note       Fill all pixels to Black
  */
void clearScreen()
{
    OffsetX = 0;
    TFT_099.DispColor(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
}



/**
  * @brief      Assign Characters
  * @param      none
  * @return     none
  * @note       moved from setup()
  */
void assignChar()
{

    // assign characters --- default
    keyboard[0][0] = 'q';
    keyboard[0][1] = 'w';
    keyboard[0][2] = NULL;  // [Symbol]
    keyboard[0][3] = 'a';
    keyboard[0][4] = NULL;  // [ALT]
    keyboard[0][5] = ' ';   // [SPACE]
    keyboard[0][6] = NULL;  // [MIC]

    keyboard[1][0] = 'e';
    keyboard[1][1] = 's';
    keyboard[1][2] = 'd';
    keyboard[1][3] = 'p';
    keyboard[1][4] = 'x';
    keyboard[1][5] = 'z';
    keyboard[1][6] = NULL;  // [LShift]

    keyboard[2][0] = 'r';
    keyboard[2][1] = 'g';
    keyboard[2][2] = 't';
    keyboard[2][3] = NULL;  // [RShift]
    keyboard[2][4] = 'v';
    keyboard[2][5] = 'c';
    keyboard[2][6] = 'f';

    keyboard[3][0] = 'u';
    keyboard[3][1] = 'h';
    keyboard[3][2] = 'y';
    keyboard[3][3] = NULL;  // [Enter]
    keyboard[3][4] = 'b';
    keyboard[3][5] = 'n';
    keyboard[3][6] = 'j';

    keyboard[4][0] = 'o';
    keyboard[4][1] = 'l';
    keyboard[4][2] = 'i';
    keyboard[4][3] = NULL;  // [Backspace]
    keyboard[4][4] = '$';
    keyboard[4][5] = 'm';
    keyboard[4][6] = 'k';

    // assign characters --- symbol
    keyboard_symbol[0][0] = '#';
    keyboard_symbol[0][1] = '1';
    keyboard_symbol[0][2] = NULL;   // [Symbol]
    keyboard_symbol[0][3] = '*';
    keyboard_symbol[0][4] = NULL;   // [ALT]
    keyboard_symbol[0][5] = NULL;   // [SPACE]
    keyboard_symbol[0][6] = '0';    // [MIC]

    keyboard_symbol[1][0] = '2';
    keyboard_symbol[1][1] = '4';
    keyboard_symbol[1][2] = '5';
    keyboard_symbol[1][3] = '@';
    keyboard_symbol[1][4] = '8';
    keyboard_symbol[1][5] = '7';
    keyboard_symbol[1][6] = NULL;   // [LShift]

    keyboard_symbol[2][0] = '3';
    keyboard_symbol[2][1] = '/';
    keyboard_symbol[2][2] = '(';
    keyboard_symbol[2][3] = NULL;   // [RShift]
    keyboard_symbol[2][4] = '?';
    keyboard_symbol[2][5] = '9';
    keyboard_symbol[2][6] = '6';

    keyboard_symbol[3][0] = '_';
    keyboard_symbol[3][1] = ':';
    keyboard_symbol[3][2] = ')';
    keyboard_symbol[3][3] = NULL;   // [Enter]
    keyboard_symbol[3][4] = '!';
    keyboard_symbol[3][5] = ',';
    keyboard_symbol[3][6] = ';';

    keyboard_symbol[4][0] = '+';
    keyboard_symbol[4][1] = '"';
    keyboard_symbol[4][2] = '-';
    keyboard_symbol[4][3] = NULL;   // [Backspace]
    keyboard_symbol[4][4] = NULL;   // [$]
    keyboard_symbol[4][5] = '.';
    keyboard_symbol[4][6] = '\'';

    delay(500);

    return;
}




/**
  * @brief      Display Status such as Caps Lock / Symbol Lock / ...
  * @param      none
  * @return     none
  * @note       note
  */
void displayKbdStatus()
{
    String  string_Status;
    char    c[6];

    int     OffsetX_DispStatus = 8;
    (case_locking) ? (string_Status = " CAP ") : (string_Status = "     ");
    strcpy(c, string_Status.c_str());
    TFT_099.DispStr(c, OffsetX_DispStatus, (GAP_Y+FONT_H+2), GREEN, BLACK);

    OffsetX_DispStatus += (5*GAP);
    (symbolSelected) ? (string_Status = " SYM ") : (string_Status = "     ");
    strcpy(c, string_Status.c_str());
    TFT_099.DispStr(c, OffsetX_DispStatus, (GAP_Y+FONT_H+2), GREEN, BLACK);

    // OffsetX_DispStatus += (5*GAP);
    // (alt_active) ? (string_Status = "[ALT]") : (string_Status = "[   ]");
    // strcpy(c, string_Status.c_str());
    // TFT_099.DispStr(c, OffsetX_DispStatus, (GAP_Y+FONT_H+2), BLUE, BLACK);

}



/**
  * @brief      Display help message to serial console
  * @param      none
  * @return     none
  * @note       note
  */
void displayHelpMsg(void)
{
    Serial.println("***************************************************************************");
    Serial.println();
    Serial.printf("\t[Help]\r\n");
    Serial.println();
    Serial.printf("  LShift       Caps Lock\r\n");
    Serial.printf("  Alt+Space    Tab\r\n");
    Serial.printf("  Alt+Rshift   Change IME\r\n");
    Serial.println();
    Serial.printf("  Alt+B        Toggle Backlight\r\n");
    Serial.printf("  Alt+I        Up    Arrow\r\n");
    Serial.printf("  Alt+K        Down  Arrow\r\n");
    Serial.printf("  Alt+J        Left  Arrow\r\n");
    Serial.printf("  Alt+L        Right Arrow\r\n");
    Serial.println();
    Serial.printf("  Alt+Z        Previous Track\r\n");
    Serial.printf("  Alt+X        Play/Pause\r\n");
    Serial.printf("  Alt+C        Next Track\r\n");
    Serial.printf("  Alt+V        Volume Down\r\n");
    Serial.printf("  Alt+N        Volume Up\r\n");
    Serial.printf("  Alt+M        Mute\r\n");
    Serial.println();
    Serial.println("***************************************************************************");
    Serial.println();
}

/**
  * @brief      Read Keyboard Matrix
  * @param      none
  * @return     none
  * @note       note
  */
void readMatrix()
{
    int delayTime = 0;      // unused local var

    // iterate the columns
    for (int colIndex = 0; colIndex < colCount; colIndex++)
    {
        // col: set to output to low
        byte curCol = cols[colIndex];
        pinMode(curCol, OUTPUT);
        digitalWrite(curCol, LOW);

        // row: iterate through the rows
        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
        {
            byte rowCol = rows[rowIndex];
            pinMode(rowCol, INPUT_PULLUP);

            delay(1); // arduino is not fast enought to switch input/output modes so wait 1 ms

            // get Keypress state
            bool buttonPressed = (digitalRead(rowCol) == LOW);

            // update current keypress state
            keys[colIndex][rowIndex] = buttonPressed;

            // determine whether state changed
            if ((lastValue[colIndex][rowIndex] != buttonPressed))
            {
                changedValue[colIndex][rowIndex] = true;    // is  changed
            }
            else
            {
                changedValue[colIndex][rowIndex] = false;   // NOT changed
            }

            // update Last Keypress State
            lastValue[colIndex][rowIndex] = buttonPressed;

            pinMode(rowCol, INPUT);
        }

        // disable the column
        pinMode(curCol, INPUT);
    }

#ifndef SYMBOL_LOCK
    // assert [SYM] state
    if (keyPressed(KEYIDX_SYM_COL, KEYIDX_SYM_ROW))
    {
        symbolSelected = true;
        // symbolSelected = !symbolSelected;
    }
#endif

}



/**
  * @brief      determine whether key is pressed
  * @param      colIndex
  * @param      rowIndex
  * @return     return true   when (changedValue[c][r] && keys[c][r]) is true
  * @note
  */
bool keyPressed(int colIndex, int rowIndex)
{
    return changedValue[colIndex][rowIndex] && keys[colIndex][rowIndex] == true;
}

/**
  * @brief      determine whether key is actived
  * @param      colIndex
  * @param      rowIndex
  * @return     return true   when (keys[c][r]) is true
  * @note
  */
bool keyActive(int colIndex, int rowIndex)
{
    return keys[colIndex][rowIndex] == true;
}

/**
  * @brief      determine whether character of pressed key is printable
  * @param      colIndex
  * @param      rowIndex
  * @return     return true   when (keyboard_symbol[c][r] && keyboard[c][r]) is true
  * @note
  */
bool isPrintableKey(int colIndex, int rowIndex)
{
    return keyboard_symbol[colIndex][rowIndex] != NULL || keyboard[colIndex][rowIndex] != NULL;
}



/**
  * @brief      Print out pressed key to Serial Monitor & TFT LCD
  * @param      none
  * @return     none
  * @note       note
  */
void printMatrix()
{

    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
    {
        for (int colIndex = 0; colIndex < colCount; colIndex++)
        {
            // we only want to print if the key is pressed and it is a printable character
            if (keyPressed(colIndex, rowIndex) && isPrintableKey(colIndex, rowIndex))
            {
                String toPrint;

                if (symbolSelected)
                {
#ifndef SYMBOL_LOCK
                    symbolSelected = false;
#endif
                    toPrint = String(keyboard_symbol[colIndex][rowIndex]);
                }
                else
                {
                    toPrint = String(keyboard[colIndex][rowIndex]);
                }

                // assert [ALT] state
                if (keyActive(KEYIDX_ALT_COL, KEYIDX_ALT_ROW))
                {
                    alt_active = true;
                    keys[KEYIDX_ALT_COL][KEYIDX_ALT_ROW] = false;
                    return;
                }

                // keys 1,6 and 2,3 are Shift keys, so we want to upper case
#ifdef CAPSLOCK_MAP_RSH
                // (caps lock state) or ([RSH] is pressed)
                if (case_locking || keyActive(KEYIDX_RSH_COL, KEYIDX_RSH_ROW))
#else
                // (caps lock state) or ([LSH] is pressed)
                if (case_locking || keyActive(KEYIDX_LSH_COL, KEYIDX_LSH_ROW))
#endif
                {
#ifdef SYMBOL_LOCK
                    if(false == symbolSelected)
#endif
                        toPrint.toUpperCase();
                }

                TFT_099.DispColor(0, OffsetX, TFT_HIGH, TFT_WIDE, BLACK);
                char c[2];
                strcpy(c, toPrint.c_str());

#ifdef ECHO_LCD_EN
                TFT_099.DispStr(c, OffsetX, GAP_Y, WHITE, BLACK);
#endif

#ifdef ECHO_SER_EN
                Serial.print(c);              // 1st printout
                Serial.printf(" ");
                Serial.println(toPrint);      // 2nd printout
#endif

                bleKeyboard.print(toPrint);   // sends keystrokes

                OffsetX = OffsetX + GAP;

                if (OffsetX > 160)
                {
                    OffsetX = 0;
                    TFT_099.DispColor(0, 0, TFT_HIGH, TFT_WIDE, BLACK);
                }

                TFT_099.backlight(50);
                previousMillis_1 = millis();

            }
        }
    }
}



/**
  * @brief      brief
  * @param      param
  * @return     return
  * @note       note
  */
