/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            main.cpp
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     MZ2500/2800 Key Matrix logic.
//                  This source file contains the application logic to obtain PS/2 scan codes, map them
//                  into a virtual keyboard matrix as per the Sharp MZ series key matrix and the
//                  logic to transmit the virtual key matrix to the MZ2500/2800.
//
//                  The application uses a modified version of the PS2KeyAdvanced 
//                  https://github.com/techpaul/PS2KeyAdvanced class from Paul Carpenter.
//
//                  The application uses, for debug purposes, the esp-idf-ssd1306 class from nopnop2002
//                  https://github.com/nopnop2002/esp-idf-ssd1306.
//
//                  The application uses the Espressif Development environment with Arduino components.
//                  This is necessary for the PS2KeyAdvanced class, which I may in future convert to
//                  use esp-idf library calls rather than Arduino.
//
//                  The Espressif environment is necessary in order to have more control over the build.
//                  It is important, for timing, that the Core 1 is dedicated to MZ Interface 
//                  logic and Core 0 is used for all RTOS/Interrupts tasks. 
//
//                  The application is configured via the Kconfig system. Use idf.py menuconfig to 
//                  configure.
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Jan 2022 - Initial write.
//
// Notes:           See Makefile to enable/disable conditional components
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// This source file is free software: you can redistribute it and#or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Arduino.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "PS2KeyAdvanced.h"
#include "MZKeyTable.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include "sdkconfig.h"

// ESP Logging tag.
#define tag "mz25key"

//////////////////////////////////////////////////////////////////////////
// Important:
//
// All configuration is performed via the 'idf.py menuconfig' command.
// Optionally override via the definitions below.
//////////////////////////////////////////////////////////////////////////
//#define CONFIG_DEBUG_OLED         !CONFIG_OLED_DISABLED
//#define CONFIG_PWRLED             25
//#define CONFIG_PS2_HW_DATAPIN     14
//#define CONFIG_PS2_HW_CLKPIN      13
//#define CONFIG_KEYMAP_WYSE_KB3926
//#define CONFIG_KEYMAP_STANDARD
//#define CONFIG_MZ_KDB0            23
//#define CONFIG_MZ_KDB1            25
//#define CONFIG_MZ_KDB2            26
//#define CONFIG_MZ_KDB3            27
//#define CONFIG_MZ_KDO0            14
//#define CONFIG_MZ_KDO1            15
//#define CONFIG_MZ_KDO2            16
//#define CONFIG_MZ_KDO3            17
//#define CONFIG_MZ_KDO4            18
//#define CONFIG_MZ_KDO5            19
//#define CONFIG_MZ_KDO6            21
//#define CONFIG_MZ_KDO7            21
//#define CONFIG_MZ_RTSNI           35
//#define CONFIG_MZ_KDI4            13
//#CONFIG_OLED_DISABLED
//#CONFIG_I2C_INTERFACE
//#CONFIG_SPI_INTERFACE
//#CONFIG_SSD1306_128x32
//#CONFIG_SSD1306_128x64
//#CONFIG_OFFSETX                   0
//#CONFIG_FLIP
//#CONFIG_SCL_GPIO                  5
//#CONFIG_SDA_GPIO                  4
//#CONFIG_RESET_GPIO                16

// Macros.
//
#define NUMELEM(a)  (sizeof(a)/sizeof(a[0]))

// Structure to manage the translated key matrix. This is updated by the ps2Interface thread and read by the mzInterface thead.
typedef struct {
    uint8_t                 strobeAll;
    uint32_t                strobeAllAsGPIO;
    uint8_t                 keyMatrix[16];
    uint32_t                keyMatrixAsGPIO[16];
} t_mzControl;
volatile t_mzControl        mzControl  = { 0xFF, 0x00000000, 
                                           { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 
                                           { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000  }
                                         };

// Instantiate base classes. First, production required objects.
PS2KeyAdvanced              Keyboard;

// Debug required objects.
SSD1306_t                   SSD1306;

// Handle to interact with the mz-2500 interface thread.
TaskHandle_t                TaskMZ25IF = NULL;
TaskHandle_t                TaskPS2IF = NULL;

// Spin lock mutex to hold a core tied to an uninterruptable method. This only works on dual core ESP32's.
static portMUX_TYPE         mzMutex  = portMUX_INITIALIZER_UNLOCKED;

#if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
// Printf to debug console terminal.
void dbgprintf(const char * format, ...)
{
    // Locals.
    va_list     ap;

    // Commence variable argument processing.
    va_start(ap, format);
    // Use vararg printf to expand and return the buffer size needed.
    int size = vsnprintf(nullptr, 0, format, ap) + 1;
    if (size > 0) 
    {
        va_end(ap);

        // Repeat and this time output the expanded string to a buffer for printing.
        va_start(ap, format);
        char buf[size + 1];
        vsnprintf(buf, size, format, ap);

        // Output to LED or console
        // tbc
    }
    va_end(ap);
}
#endif

// Method to connect and interact with the MZ-2500/MZ-2800 keyboard controller.
// The basic requirement is to:
//   1. Detect a falling edge on the RTSN signal
//   2. Read the provided ROW number.
//   3. Lookup the matrix data for given ROW.
//   4. Output data to LS257 Mux.
//   5. Loop
//
//   The ps2Interface method is responsible for obtaining a PS/2 Keyboard scancode and
//   creating the corresponding virtual matrix.
//
//   NB: As this method holds Core 1 under spinlock, no FreeRTOS or Arduino access 
//   can be made except for basic I/O ports. The spinlock has to be released for non
//   I/O work.
//
// The MZ timing period is ~600ns RTSN Low to High (Latch Row data coming from MZ machine),
// MPX (connected direct to external Quad 2-1 Mux goes high as RTSN goes low, the MPX
// signal is held high for 300ns then goes low for 300ns. The period when RTSN is low is
// when the MZ machine reads the scan row data. The cycle is ~1.2uS repeating, 14 rows
// so ~16.8uS for one full key matrix, The MZ machine if stuck in a tight loop will take
// approx 100uS to scan the matrix so the Gate Arrays are over sampling 6 times.
//
IRAM_ATTR void mz25Interface( void * pvParameters )
{
    // Locals.
    volatile uint32_t gpioIN;
    volatile uint8_t  strobeRow = 0;
    uint32_t          rowBitMask = (1 << CONFIG_MZ_KDB3) | (1 << CONFIG_MZ_KDB2) | (1 << CONFIG_MZ_KDB1) | (1 << CONFIG_MZ_KDB0);
    uint32_t          colBitMask = (1 << CONFIG_MZ_KDO7) | (1 << CONFIG_MZ_KDO6) | (1 << CONFIG_MZ_KDO5) | (1 << CONFIG_MZ_KDO4) | 
                                   (1 << CONFIG_MZ_KDO3) | (1 << CONFIG_MZ_KDO2) | (1 << CONFIG_MZ_KDO1) | (1 << CONFIG_MZ_KDO0);
    uint32_t          pwrLEDMask = (1 << CONFIG_PWRLED);

    ESP_LOGI(tag, "Starting mz25Interface thread, colBitMask=%08x, rowBitMask=%08x.", colBitMask, rowBitMask);

    // Create, initialise and hold a spinlock so the current core is bound to this one method.
    portENTER_CRITICAL(&mzMutex);

    // Permanent loop, just wait for an RTSN strobe, latch the row, lookup matrix and output.
    // Timings with Power LED = LED Off to On = 108ns, LED On to Off = 392ns
    for(;;)
    {
        // Turn on Power LED.
        GPIO.out_w1ts = pwrLEDMask;

        // Read the GPIO ports to get latest RTSNi and KDI4 states.
        gpioIN = REG_READ(GPIO_IN_REG);

        // Detect RTSN going high, the MZ will send the required row during this cycle.
        if(gpioIN & CONFIG_MZ_RTSNI)
        {
            // Assemble the required matrix row from the configured bits.
            strobeRow = (gpioIN >> (CONFIG_MZ_KDB3-3)) | (gpioIN >> (CONFIG_MZ_KDB2-2)) | (gpioIN >> (CONFIG_MZ_KDB1-1)) | (gpioIN >> CONFIG_MZ_KDB0);
         
            // Clear all KDO bits - clear state = '1'
            GPIO.out_w1ts = colBitMask;                            // Reset all scan data bits to '1', inactive.

            // KDI4 indicates if row data is needed or a single byte ANDing all the keys together, ie. to detect a key press without strobing all rows.
            if(gpioIN & CONFIG_MZ_KDI4)
            {
                // Set all required KDO bits according to keyMatrix, set state = '0'.
                GPIO.out_w1tc = mzControl.keyMatrixAsGPIO[strobeRow];  // Set to '0' active bits.
            } else
            {
                // Set all required KDO bits according to the strobe all value. set state = '0'.
                GPIO.out_w1tc = mzControl.strobeAllAsGPIO;             // Set to '0' active bits.
            }

            // Wait for RTSN to go low.
            while(REG_READ(GPIO_IN_REG) & CONFIG_MZ_RTSNI);
        }

        // Turn off Power LED.
        GPIO.out_w1tc = pwrLEDMask;

        // Logic to feed the watchdog if needed. Watchdog disabled in menuconfig but if enabled this will need to be used.
        //TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG0.wdt_feed=1;                       // feed dog
        //TIMERG0.wdt_wprotect=0;                   // write protect
        //TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
        //TIMERG1.wdt_feed=1;                       // feed dog
        //TIMERG1.wdt_wprotect=0;                   // write protect
    }
}

// Method to convert the PS2 scan code into a key matrix representation which the MZ-2500/2800 is expecting.
//
IRAM_ATTR unsigned char updateMatrix(uint16_t data)
{
    // Locals.
    uint8_t   idx;
    uint8_t   idx2;
    uint8_t   changed = 0;

    // Loop through the entire conversion table to find a match on this key, if found appy the conversion to the virtual
    // switch matrix.
    //
    for(idx=0; idx < NUMELEM(PS2toMZ); idx++)
    {
        // Match key code?
        if(PS2toMZ[idx][PSMZTBL_KEYPOS] == (uint8_t)(data&0xFF))
        {
            // Match Raw, Shift, Function, Control, ALT or ALT-Gr?
            if( (PS2toMZ[idx][PSMZTBL_SHIFTPOS] == 0 && PS2toMZ[idx][PSMZTBL_FUNCPOS] == 0 && PS2toMZ[idx][PSMZTBL_CTRLPOS] == 0 && PS2toMZ[idx][PSMZTBL_ALTPOS] == 0 && PS2toMZ[idx][PSMZTBL_ALTGRPOS] == 0) ||
                ((data & PS2_SHIFT)    && PS2toMZ[idx][PSMZTBL_SHIFTPOS] == 1) || 
                ((data & PS2_FUNCTION) && PS2toMZ[idx][PSMZTBL_FUNCPOS]  == 1) ||
                ((data & PS2_CTRL)     && PS2toMZ[idx][PSMZTBL_CTRLPOS]  == 1) ||
                ((data & PS2_ALT)      && PS2toMZ[idx][PSMZTBL_ALTPOS]   == 1) ||
                ((data & PS2_ALT_GR)   && PS2toMZ[idx][PSMZTBL_ALTGRPOS] == 1) )
            {
                // RELEASE (PS2_BREAK == 1) or PRESS?
                if((data & PS2_BREAK))
                {
                    // Reset the matrix bit according to the lookup table. 1 = No key, 0 = key in the matrix.
                    if(PS2toMZ[idx][PSMZTBL_MXROW1] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW1]] |= PS2toMZ[idx][PSMZTBL_MXKEY1];
                        changed = 1;
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW2] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW2]] |= PS2toMZ[idx][PSMZTBL_MXKEY2];
                        changed = 1;
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW3] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW3]] |= PS2toMZ[idx][PSMZTBL_MXKEY3];
                        changed = 1;
                    }
                } else
                {
                    // Set the matrix bit according to the lookup table. 1 = No key, 0 = key in the matrix.
                    if(PS2toMZ[idx][PSMZTBL_MXROW1] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW1]] &= ~PS2toMZ[idx][PSMZTBL_MXKEY1];
                        changed = 1;
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW2] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW2]] &= ~PS2toMZ[idx][PSMZTBL_MXKEY2];
                        changed = 1;
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW3] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW3]] &= ~PS2toMZ[idx][PSMZTBL_MXKEY3];
                        changed = 1;
                    }
                }
            }
           
            // Only spend timw updating signals if an actual change occurred. Some keys arent valid so no change will be effected.
            if(changed)
            {
                // To save time in the MZ Interface, a mirror keyMatrix is built up, 32bit (GPIO Bank 0) wide, with the keyMatrix 8 bit data
                // mapped onto the configured pins in the 32bit register. This saves previous time in order to meet the tight 1.2uS cycle.
                //
                for(int idx=0; idx < 15; idx++)
                {
                    mzControl.keyMatrixAsGPIO[idx] =  (((mzControl.keyMatrix[idx] >> 7) & 0x01) ^ 0x01) << CONFIG_MZ_KDO7 |
                                                      (((mzControl.keyMatrix[idx] >> 6) & 0x01) ^ 0x01) << CONFIG_MZ_KDO6 |
                                                      (((mzControl.keyMatrix[idx] >> 5) & 0x01) ^ 0x01) << CONFIG_MZ_KDO5 |
                                                      (((mzControl.keyMatrix[idx] >> 4) & 0x01) ^ 0x01) << CONFIG_MZ_KDO4 |
                                                      (((mzControl.keyMatrix[idx] >> 3) & 0x01) ^ 0x01) << CONFIG_MZ_KDO3 |
                                                      (((mzControl.keyMatrix[idx] >> 2) & 0x01) ^ 0x01) << CONFIG_MZ_KDO2 |
                                                      (((mzControl.keyMatrix[idx] >> 1) & 0x01) ^ 0x01) << CONFIG_MZ_KDO1 |
                                                      (((mzControl.keyMatrix[idx]     ) & 0x01) ^ 0x01) << CONFIG_MZ_KDO0 ;
                }

                // Re-calculate the Strobe All (KD4 = 1) signal, this indicates if any bit (key) in the matrix is active.
                mzControl.strobeAll = 0xFF;
                mzControl.strobeAllAsGPIO = 0x00000000;
                for(idx2=0; idx2 < 15; idx2++)
                {
                    mzControl.strobeAll &= mzControl.keyMatrix[idx2];
                }

                // To speed up the mzInterface logic, pre-calculate the strobeAll value as a 32bit GPIO output value.
                mzControl.strobeAllAsGPIO |= (((mzControl.strobeAll >> 7) & 0x01) ^ 0x01) << CONFIG_MZ_KDO7 |
                                             (((mzControl.strobeAll >> 6) & 0x01) ^ 0x01) << CONFIG_MZ_KDO6 |
                                             (((mzControl.strobeAll >> 5) & 0x01) ^ 0x01) << CONFIG_MZ_KDO5 |
                                             (((mzControl.strobeAll >> 4) & 0x01) ^ 0x01) << CONFIG_MZ_KDO4 |
                                             (((mzControl.strobeAll >> 3) & 0x01) ^ 0x01) << CONFIG_MZ_KDO3 |
                                             (((mzControl.strobeAll >> 2) & 0x01) ^ 0x01) << CONFIG_MZ_KDO2 |
                                             (((mzControl.strobeAll >> 1) & 0x01) ^ 0x01) << CONFIG_MZ_KDO1 |
                                             (((mzControl.strobeAll     ) & 0x01) ^ 0x01) << CONFIG_MZ_KDO0 ;
            }
        }
    }
    return changed;
}

// Primary PS/2 thread, running on Core 1.
// This thread is responsible for receiving PS/2 scan codes and mapping them to an MZ-2500/2800 keyboard matrix.
// The PS/2 data is received via interrupt.
//
IRAM_ATTR void ps2Interface( void * pvParameters )
{
    // Locals.
    uint16_t            scanCode   = 0x0000;
    #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
      uint8_t           dataChange = 0;
      static int        clrScreen  = 1;
      static int        scanPrtCol = 0;
      static uint32_t   clrTimer   = 0;
    #endif

    #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
        if((clrTimer > 0 && --clrTimer == 0) || ((scanCode&0xFF) == PS2_KEY_C && scanCode & PS2_BREAK ))
        {
            // Clear old scan code data. Add OLED code if needed. 
            scanPrtCol = 0;
        }
    #endif

    while(1)
    { 
        // Check for PS/2 keyboard scan codes.
        while((scanCode = Keyboard.read()) != 0)
        {
            printf("%04x\n", scanCode);
            #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
                // Clear screen as requested.
                if(clrScreen == 1)
                {
                //  ssd1306_clear_screen(&SSD1306, false);
                    clrScreen = 0;
                }

                // Output the scan code for verification.
                dbgprintf("%04x,", scanCode);
                if(scanPrtCol++ >= 3) scanPrtCol = 0;
                clrTimer = 2000000;
            #endif

            // Update the virtual matrix with the new key value.
            dataChange = updateMatrix(scanCode);

            if(dataChange)
            {
                // To save time in the MZ Interface, a mirror keyMatrix is built up, 32bit (GPIO Bank 0) wide, with the keyMatrix 8 bit data
                // mapped onto the configured pins in the 32bit register. This saves previous time in order to meet the tight 1.2uS cycle.
                //
                for(int idx=0; idx < 15; idx++)
                {
                    mzControl.keyMatrixAsGPIO[idx] =  (((mzControl.keyMatrix[idx] >> 7) & 0x01) ^ 0x01) << CONFIG_MZ_KDO7 |
                                                      (((mzControl.keyMatrix[idx] >> 6) & 0x01) ^ 0x01) << CONFIG_MZ_KDO6 |
                                                      (((mzControl.keyMatrix[idx] >> 5) & 0x01) ^ 0x01) << CONFIG_MZ_KDO5 |
                                                      (((mzControl.keyMatrix[idx] >> 4) & 0x01) ^ 0x01) << CONFIG_MZ_KDO4 |
                                                      (((mzControl.keyMatrix[idx] >> 3) & 0x01) ^ 0x01) << CONFIG_MZ_KDO3 |
                                                      (((mzControl.keyMatrix[idx] >> 2) & 0x01) ^ 0x01) << CONFIG_MZ_KDO2 |
                                                      (((mzControl.keyMatrix[idx] >> 1) & 0x01) ^ 0x01) << CONFIG_MZ_KDO1 |
                                                      (((mzControl.keyMatrix[idx]     ) & 0x01) ^ 0x01) << CONFIG_MZ_KDO0 ;
                }

                #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
                    // Output the MZ virtual keyboard matrix for verification.
                    uint8_t oledBuf[8][16];
                    for(int idx=0; idx < 15; idx++)
                    {
                        for(int idx2=0; idx2 < 8; idx2++)
                        {
                            oledBuf[idx2][idx] = ((mzControl.keyMatrix[idx] >> idx2)&0x01) == 1 ? '1' : '0';
                        }
                    }

                    // Print out the matrix, transposed - see MZKeyTable.h for the map, second table.
                    for(int idx=0; idx < 8; idx++)
                    {
                        ssd1306_display_text(&SSD1306, idx, (char *)oledBuf[idx], 15, false);
                    }
                #endif
            }
        }

        // Let other tasks run.
        vTaskDelay(0);
   }
}


// Setup method to configure ports, devices and threads prior to application run.
// Configuration:
//      PS/2 Keyboard over 2 wire interface
//      Power/Status LED
//      Optional OLED debug output screen
//      4 bit input - MZ-2500/2800 Row Number
//      8 bit output - MZ-2500/2800 Scan data
//      1 bit input  - RTSN strobe line, low indicating a new Row Number available.
//      1 bit input  - KD4, High = Key scan data required, Low = AND of all key matrix rows required.
//
void setup()
{
    // Locals.
    gpio_config_t io_conf;
   
    // Setup power LED first to show life.
    ESP_LOGI(tag, "Configuring Power LED.");
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT; 
    io_conf.pin_bit_mask = (1ULL<<CONFIG_PWRLED); 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)CONFIG_PWRLED, 1);

    // Start the keyboard, no mouse.
    ESP_LOGI(tag, "Initialise PS2 keyboard.");
    Keyboard.begin(CONFIG_PS2_HW_DATAPIN, CONFIG_PS2_HW_CLKPIN);

    // If OLED connected and enabled, include the screen controller for debug output.
    //
    #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
        #if CONFIG_I2C_INTERFACE
            ESP_LOGI(tag, "INTERFACE is i2c");
            ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",     CONFIG_SDA_GPIO);
            ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",     CONFIG_SCL_GPIO);
            ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",   CONFIG_RESET_GPIO);
            i2c_master_init(&SSD1306, CONFIG_SDA_GPIO,  CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
        #endif // CONFIG_I2C_INTERFACE
        #if CONFIG_SPI_INTERFACE
            ESP_LOGI(tag, "INTERFACE is SPI");
            ESP_LOGI(tag, "CONFIG_MOSI_GPIO=%d",    CONFIG_MOSI_GPIO);
            ESP_LOGI(tag, "CONFIG_SCLK_GPIO=%d",    CONFIG_SCLK_GPIO);
            ESP_LOGI(tag, "CONFIG_CS_GPIO=%d",      CONFIG_CS_GPIO);
            ESP_LOGI(tag, "CONFIG_DC_GPIO=%d",      CONFIG_DC_GPIO);
            ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",   CONFIG_RESET_GPIO);
            spi_master_init(&SSD1306, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
        #endif // CONFIG_SPI_INTERFACE

        #if CONFIG_SSD1306_128x64
            ESP_LOGI(tag, "Panel is 128x64");
            ssd1306_init(&SSD1306, 128, 64);
        #endif // CONFIG_SSD1306_128x64
        #if CONFIG_SSD1306_128x32
            ESP_LOGI(tag, "Panel is 128x32");
            ssd1306_init(&SSD1306, 128, 32);
        #endif // CONFIG_SSD1306_128x32

        ssd1306_clear_screen(&SSD1306, false);
        ssd1306_contrast(&SSD1306, 0xff);
    #endif

    // Configure 4 inputs to be the Strobe Row Number which is used to index the virtual key matrix and the strobe data returned.
    #if !defined(CONFIG_MZ_DISABLE_KDB)
        ESP_LOGI(tag, "Configuring MZ-2500/2800 4 bit Row Number Inputs.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDB0); 
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDB1); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDB2); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDB3); 
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_MZ_DISABLE_KDO)
        ESP_LOGI(tag, "Configuring MZ-2500/2800 8 bit Strobe data Outputs.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_OUTPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO0); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO1); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO2); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO3); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO4); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO5); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO6); 
        gpio_config(&io_conf);
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDO7); 
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_MZ_DISABLE_KDI)
        ESP_LOGI(tag, "Configuring MZ-2500/2800 RTSN Input.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_RTSNI); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_MZ_DISABLE_RTSNI)
        ESP_LOGI(tag, "Configuring MZ-2500/2800 KD4 Input.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDI4); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    // Check to see if keyboard available, no keyboard = no point, so halt!
    // Firstly, ping keyboard to see if it is there.
    ESP_LOGI(tag, "Detecting PS2 keyboard.");
    Keyboard.echo();              
    vTaskDelay(6);
    uint16_t chr = Keyboard.read();
    if( (chr & 0xFF) != PS2_KEY_ECHO && (chr & 0xFF) != PS2_KEY_BAT)
    {
        ESP_LOGE(tag, "No PS2 keyboard detected, connect and reset to continue.\n");
        #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
            ssd1306_display_text(&SSD1306, 0, "No PS2 Keyboard", 15, false);
        #endif
        while(1);
    }

    // Create a task pinned to core 0 which will fulfill the MZ-2500/2800 interface. This task has the highest priority
    // and it will also hold spinlock and manipulate the watchdog to ensure a scan cycle timing can be met. This means 
    // all other tasks running on Core 1 will suspend. The PS/2 controller, running on the ULP processor will continue
    // to interact with the PS/2 keyboard and buffer scan codes.
    //
    // Core 1 - MZ Interface
    ESP_LOGI(tag, "Starting mz25if thread...");
    xTaskCreatePinnedToCore(mz25Interface, "mz25if", 32768, NULL, 25, &TaskMZ25IF, 1);
    vTaskDelay(500);

    // Core 0 - Application
    ESP_LOGI(tag, "Starting ps2if thread...");
    xTaskCreatePinnedToCore(ps2Interface, "ps2if",   32768, NULL, 22, &TaskPS2IF, 0);
    vTaskDelay(500);
}

// ESP-IDF Application entry point.
//
extern "C" void app_main()
{
    // Arduino runtime support isnt needed.
    //initArduino();

    // Setup hardware and start primary control threads,
    setup();
}
