/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            mz25key.ino
// Created:         Jan 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     MZ2500/2800 Key Matrix logic.
//                  This source file contains the logic to transmit the virtual key matrix, which is 
//                  built from PS/2 scan codes, to the MZ2500/2800.
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
// Debugging options.
//////////////////////////////////////////////////////////////////////////
//#define CONFIG_DEBUG_OLED         !CONFIG_OLED_DISABLED
//#define CONFIG_PWRLED             25
//#define CONFIG_PS2_HW_DATAPIN     14
//#define CONFIG_PS2_HW_CLKPIN      13

// Macros.
//
#define NUMELEM(a)  (sizeof(a)/sizeof(a[0]))

// Structure to manage the translated key matrix. This is updated by the ps2Interface thread and read by the mzInterface thead.
typedef struct {
    uint8_t                 strobeAll;
    uint8_t                 keyMatrix[15];
} t_mzControl;
volatile t_mzControl        mzControl  = { 0xFF, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

// Instantiate base classes. First, production required objects.
PS2KeyAdvanced              Keyboard;
// Nex/t, debug required objects.
SSD1306_t                   SSD1306;

// Handle to interact with the mz-2500 interface thread.
TaskHandle_t                TaskMZ25IF = NULL;
TaskHandle_t                TaskPS2IF = NULL;

// Spin lock mutex to hold a core tied to an uninterruptable method. This only works on dual core ESP32's.
static portMUX_TYPE         mzMutex  = portMUX_INITIALIZER_UNLOCKED;

#if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
// Printf to terminal, needed when OLED is connected for debugging.
void terminalPrintf(const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    int size = vsnprintf(nullptr, 0, format, ap) + 1;
    if (size > 0) 
    {
        va_end(ap);
        va_start(ap, format);
        char buf[size + 1];
        vsnprintf(buf, size, format, ap);
   //     u8g2.print(buf);
       // u8g2.sendBuffer();
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
IRAM_ATTR void mz25Interface( void * pvParameters )
{
    // Locals.
    volatile unsigned long idx;

    // Create, initialise and hold a spinlock so the current core is bound to this one method.
    portENTER_CRITICAL(&mzMutex);

    // Permanent loop, just wait for an RTSN strobe, latch the row, lookup matrix and output.
    for(;;)
    {
        gpio_set_level((gpio_num_t)CONFIG_PWRLED, 1);
       // digitalWrite(CONFIG_PWRLED, HIGH);
        for(idx=0; idx < 10000000; idx++)
        {
            if(idx % 1000 == 0)
            {
                TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
                TIMERG0.wdt_feed=1;                       // feed dog
                TIMERG0.wdt_wprotect=0;                   // write protect
                TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
                TIMERG1.wdt_feed=1;                       // feed dog
                TIMERG1.wdt_wprotect=0;                   // write protect
            }
        }
       // digitalWrite(CONFIG_PWRLED, LOW);
        gpio_set_level((gpio_num_t)CONFIG_PWRLED, 0);
        for(idx=0; idx < 10000000; idx++)
        {
            if(idx % 1000 == 0)
            {
                TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
                TIMERG0.wdt_feed=1;                       // feed dog
                TIMERG0.wdt_wprotect=0;                   // write protect
                TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
                TIMERG1.wdt_feed=1;                       // feed dog
                TIMERG1.wdt_wprotect=0;                   // write protect
            }
        }
    }
}

// Method to convert the PS2 scan code into a key matrix representation which the MZ-2500/2800 is expecting.
//
IRAM_ATTR unsigned char updateMatrix(uint16_t data)
{
    // Locals.
    uint8_t   idx;
    uint8_t   idx2;

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
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW2] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW2]] |= PS2toMZ[idx][PSMZTBL_MXKEY2];
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW3] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW3]] |= PS2toMZ[idx][PSMZTBL_MXKEY3];
                    }
                } else
                {
                    // Set the matrix bit according to the lookup table. 1 = No key, 0 = key in the matrix.
                    if(PS2toMZ[idx][PSMZTBL_MXROW1] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW1]] &= ~PS2toMZ[idx][PSMZTBL_MXKEY1];
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW2] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW2]] &= ~PS2toMZ[idx][PSMZTBL_MXKEY2];
                    }
                    if(PS2toMZ[idx][PSMZTBL_MXROW3] != 0xFF)
                    {
                        mzControl.keyMatrix[PS2toMZ[idx][PSMZTBL_MXROW3]] &= ~PS2toMZ[idx][PSMZTBL_MXKEY3];
                    }
                }
            }

            // Re-calculate the Strobe All (KD4 = 1) signal, this indicates if any bit (key) in the matrix is active.
            mzControl.strobeAll = 0xFF;
            for(idx2=0; idx2 < 15; idx2++)
            {
                mzControl.strobeAll &= mzControl.keyMatrix[idx2];
            }
        }
    }
    return data;
}

// Primary PS/2 thread, running on Core 1.
// This thread is responsible for receiving PS/2 scan codes and mapping them to an MZ-2500/2800 keyboard matrix.
// The PS/2 data is received via interrupt.
//
IRAM_ATTR void ps2Interface( void * pvParameters )
//IRAM_ATTR void ps2Interface( )
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
            // Clear old scan code data.
     //       u8g2.drawStr(0, 8*7, "                       ");
     //       u8g2.sendBuffer();
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
                    ssd1306_clear_screen(&SSD1306, false);
                    clrScreen = 0;
                }

                // Output the scan code for verification.
        //      u8g2.setCursor((scanPrtCol*5)*6, 8*7);
                terminalPrintf("%04x,", scanCode);
                if(scanPrtCol++ >= 3) scanPrtCol = 0;
         //     u8g2.sendBuffer();
                clrTimer = 2000000;
                dataChange = 1;
            #endif

            // Update the virtual matrix with the new key value.
            updateMatrix(scanCode);
        }

        #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
            // Output the MZ virtual keyboard matrix for verification.
            uint8_t oledBuf[8][16];
            if(dataChange)
            {
                for(int idx=0; idx < 15; idx++)
                {
                    for(int idx2=0; idx2 < 8; idx2++)
                    {
                        oledBuf[idx2][idx] = ((mzControl.keyMatrix[idx] >> idx2)&0x01) == 1 ? '1' : '0';
                    }
                }

                for(int idx=0; idx < 8; idx++)
                {
                    ssd1306_display_text(&SSD1306, idx, (char *)oledBuf[idx], 15, false);
                }
            }
        #endif
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

    // Setup power LED.
    //pinMode(CONFIG_PWRLED, OUTPUT);
    //#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
    ESP_LOGI(tag, "Configuring Power LED.");
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT; 
    io_conf.pin_bit_mask = (1ULL<<CONFIG_PWRLED); 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Configure 4 inputs to be the Strobe Row Number which is used to index the virtual key matrix and the strobe data returned.
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

    ESP_LOGI(tag, "Configuring MZ-2500/2800 RTSN Input.");
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_INPUT; 
    io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_RTSNI); 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    ESP_LOGI(tag, "Configuring MZ-2500/2800 KD4 Input.");
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_INPUT; 
    io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDI4); 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

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
    ESP_LOGI(tag, "Starting mz25if thread...");
    xTaskCreatePinnedToCore(mz25Interface, "mz25if", 32768, NULL, 25, &TaskMZ25IF, 1);
    vTaskDelay(500);
    ESP_LOGI(tag, "Starting ps2if thread...");
    xTaskCreatePinnedToCore(ps2Interface, "ps2if",   32768, NULL, 22, &TaskPS2IF, 0);
    vTaskDelay(500);
}

extern "C" void app_main()
{
 //   initArduino();
    // Setup hardware and start primary control threads,
    setup();

    // Nothing to do, yield CPU.
    while(1) {
      vTaskDelay(10000);
    }
}
