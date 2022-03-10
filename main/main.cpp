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
//                  It is important, for timing, that Core 1 is dedicated to MZ Interface 
//                  logic and Core 0 is used for all RTOS/Interrupts tasks. 
//
//                  The application is configured via the Kconfig system. Use 'idf.py menuconfig' to 
//                  configure.
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Jan 2022 - Initial write.
//                  Feb 2022 - Updates and fixes. Added logic to detect PS/2 disconnect and reconnect,
//                             added 3 alternative maps selected by ALT+F1 (MZ2500), ALT+F2(MZ2000)
//                             ALT+F3(MZ80B) due to slight differences in the key layout.
//                             Added framework for wifi so that the interface can enter AP mode to
//                             acquire local net parameters then connect to local net. Needs the web
//                             interface writing.
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
#if defined(CONFIG_MZ_WIFI_ENABLED)
  #include "freertos/event_groups.h"
  #include "esp_system.h"
  #include "esp_wifi.h"
  #include "esp_event.h"
  #include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#endif
#include "Arduino.h"
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "PS2KeyAdvanced.h"
#include "MZKeyTable.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include "sdkconfig.h"

//////////////////////////////////////////////////////////////////////////
// Important:
//
// All configuration is performed via the 'idf.py menuconfig' command.
// The file 'sdkconfig' contains the configured parameter defines.
//////////////////////////////////////////////////////////////////////////

// Macros.
//
#define NUMELEM(a)  (sizeof(a)/sizeof(a[0]))

// Structure to manage the translated key matrix. This is updated by the ps2Interface thread and read by the mzInterface thead.
typedef struct {
    uint8_t                     strobeAll;
    uint32_t                    strobeAllAsGPIO;
    uint8_t                     keyMatrix[16];
    uint32_t                    keyMatrixAsGPIO[16];
    uint8_t                     activeKeyMap;
} t_mzControl;
volatile t_mzControl            mzControl  = { 0xFF, 0x00000000, 
                                               { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 
                                               { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000  },
                                               MZ_ALL
                                             };

// Instantiate base classes. First, production required objects.
PS2KeyAdvanced                  Keyboard;

// Debug required objects.
SSD1306_t                       SSD1306;

// Handle to interact with the mz-2500/mz-2800 interface, ps/2 interface and wifi threads.
#if defined(CONFIG_MODEL_MZ2500)
    TaskHandle_t                TaskMZ25IF = NULL;
#endif
#if defined(CONFIG_MODEL_MZ2800)
    TaskHandle_t                TaskMZ28IF = NULL;
#endif
TaskHandle_t                    TaskPS2IF  = NULL;
#if defined(CONFIG_MZ_WIFI_ENABLED)
    TaskHandle_t                TaskWIFI   = NULL;
#endif

// Spin lock mutex to hold a core tied to an uninterruptable method. This only works on dual core ESP32's.
static portMUX_TYPE             mzMutex  = portMUX_INITIALIZER_UNLOCKED;

// Tag for ESP main application logging.
#define                         MAINTAG  "mz25key"

#if defined(CONFIG_MZ_WIFI_ENABLED)
    // The event group allows multiple bits for each event, but we only care about two events:
    // - we are connected to the AP with an IP
    // - we failed to connect after the maximum amount of retries 
    #define WIFI_CONNECTED_BIT  BIT0
    #define WIFI_FAIL_BIT       BIT1

    // Tag for ESP WiFi logging.
    #define                     WIFITAG  "wifi"

    // Menu selection types.
    enum WIFIMODES {
        WIFI_OFF              = 0x00,                             // WiFi is disabled.
        WIFI_ON               = 0x01,                             // WiFi is enabled.
        WIFI_CONFIG_AP        = 0x02                              // WiFi is set to enable Access Point to configure WiFi settings.
    };

    // Flag to indicate WiFi is active. Whilst active the MZ25 interface cannot run as it has to 
    // free the core. Need to find a way around this to make wifi only work on one core!
    static    bool              wifiActivated = 0;

    // FreeRTOS event group to signal when we are connected
    static EventGroupHandle_t   s_wifi_event_group;


    static int                  clientRetryCnt = 0;
#endif

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
    
            // Output to LED or console, currently via printf!
            printf(buf);
        }
        va_end(ap);
    }
#else
    #define dbgprintf(a, ...) {};
#endif

// Method to connect and interact with the MZ-5800 keyboard controller. This method is seperate from the MZ-2800
// as the scan is different and as it is time critical it needs to be per target machine.
//
// The basic requirement is to:
//   1. Detect a falling edge on the RTSN signal
//   2. Read the provided ROW number.
//   3. Lookup the matrix data for given ROW.
//   4. Output data to LS257 Mux.
//   5. Wait for RTSN to return inactive.
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
// WARNING: The GPIO's are configurable via menuconfig BUT it is assumed all except RTSNi
//          are in the first GPIO bank and RTSNi is in the second GPIO bank. Modify the code
//          if RTSNi is set in the first bank or KDB[3:0], KDI4 are in the second bank.
//
#if defined(CONFIG_MODEL_MZ2500)
    IRAM_ATTR void mz25Interface( void * pvParameters )
    {
        // Locals.
        volatile uint32_t gpioIN;
        volatile uint8_t  strobeRow = 1;
    
        // Mask values declared as variables, let the optimiser decide wether they are constants or placed in-memory.
        uint32_t          rowBitMask = (1 << CONFIG_MZ_KDB3) | (1 << CONFIG_MZ_KDB2) | (1 << CONFIG_MZ_KDB1) | (1 << CONFIG_MZ_KDB0);
        uint32_t          colBitMask = (1 << CONFIG_MZ_KDO7) | (1 << CONFIG_MZ_KDO6) | (1 << CONFIG_MZ_KDO5) | (1 << CONFIG_MZ_KDO4) | 
                                       (1 << CONFIG_MZ_KDO3) | (1 << CONFIG_MZ_KDO2) | (1 << CONFIG_MZ_KDO1) | (1 << CONFIG_MZ_KDO0);
        uint32_t          KDB3_MASK  = (1 << CONFIG_MZ_KDB3);
        uint32_t          KDB2_MASK  = (1 << CONFIG_MZ_KDB2);
        uint32_t          KDB1_MASK  = (1 << CONFIG_MZ_KDB1);
        uint32_t          KDB0_MASK  = (1 << CONFIG_MZ_KDB0);
        uint32_t          KDI4_MASK  = (1 << CONFIG_MZ_KDI4);
        uint32_t          RTSNI_MASK = (1 << (CONFIG_MZ_RTSNI - 32));
    
        ESP_LOGI(MAINTAG, "Starting mz25Interface thread, colBitMask=%08x, rowBitMask=%08x.", colBitMask, rowBitMask);
    
        // Create, initialise and hold a spinlock so the current core is bound to this one method.
        portENTER_CRITICAL(&mzMutex);
    
        // Permanent loop, just wait for an RTSN strobe, latch the row, lookup matrix and output.
        // Timings with Power LED = LED Off to On = 108ns, LED On to Off = 392ns
        for(;;)
        {
            #if defined(CONFIG_MZ_WIFI_ENABLED)
                // Whilst Wifi is active, suspend processing as we need to free up the core.
                if(wifiActivated)
                {
                    portEXIT_CRITICAL(&mzMutex);
                    while(wifiActivated);
                portENTER_CRITICAL(&mzMutex);
                }
            #endif
    
            // Detect RTSN going high, the MZ will send the required row during this cycle.
            if(REG_READ(GPIO_IN1_REG) & RTSNI_MASK)
            {
                // Read the GPIO ports to get latest Row and KDI4 states.
                gpioIN = REG_READ(GPIO_IN_REG);
    
                // Assemble the required matrix row from the configured bits.
                strobeRow = ((gpioIN&KDB3_MASK) >> (CONFIG_MZ_KDB3-3)) | ((gpioIN&KDB2_MASK) >> (CONFIG_MZ_KDB2-2)) | ((gpioIN&KDB1_MASK) >> (CONFIG_MZ_KDB1-1)) | ((gpioIN&KDB0_MASK) >> CONFIG_MZ_KDB0);
             
                // Clear all KDO bits - clear state = '1'
                GPIO.out_w1ts = colBitMask;                                // Reset all scan data bits to '1', inactive.
    
                // KDI4 indicates if row data is needed or a single byte ANDing all the keys together, ie. to detect a key press without strobing all rows.
                if(gpioIN & KDI4_MASK)
                {
                    // Set all required KDO bits according to keyMatrix, set state = '0'.
                    GPIO.out_w1tc = mzControl.keyMatrixAsGPIO[strobeRow];  // Set to '0' active bits.
                } else
                {
                    // Set all required KDO bits according to the strobe all value. set state = '0'.
                    GPIO.out_w1tc = mzControl.strobeAllAsGPIO;             // Set to '0' active bits.
                }
    
                // Wait for RTSN to go low. No lockup guarding as timing is critical also the watchdog is disabled, if RTSN never goes low then the user has probably unplugged the interface!
                while(REG_READ(GPIO_IN1_REG) & RTSNI_MASK);
            }
    
            // Logic to feed the watchdog if needed. Watchdog disabled in menuconfig but if enabled this will need to be used.
            //TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
            //TIMERG0.wdt_feed=1;                       // feed dog
            //TIMERG0.wdt_wprotect=0;                   // write protect
            //TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
            //TIMERG1.wdt_feed=1;                       // feed dog
            //TIMERG1.wdt_wprotect=0;                   // write protect
        }
    }
#endif

// Method to connect and interact with the MZ-2800 keyboard controller. This method is seperate from the MZ-2500
// as the scan is different and as it is time critical it needs to be per target machine.
//
// The basic requirement is to:
//   1. Detect a rising edge on the RTSN signal
//   2. Wait at least 200ns before sampling KD4
//   3. Wait at least 650ns before reading ROW.
//   4. Read the provided ROW number.
//   5. If KD4 = 0 then output logical AND of all columns to LS257 Mux.
//   6. If KD4 = 1 then lookup data for given row and output to LS257 Mux.
//   7. Wait for RTSN to return low.
//   7. Loop
//
//   The ps2Interface method is responsible for obtaining a PS/2 Keyboard scancode and
//   creating the corresponding virtual matrix.
//
//   NB: As this method holds Core 1 under spinlock, no FreeRTOS or Arduino access 
//   can be made except for basic I/O ports. The spinlock has to be released for non
//   I/O work.
//
// The MZ 2800 timing period is 1.78uS RTSN going active high, KD4 changing state 150ns after RTSN goes active,
// ROW number being set 650ns after RTSN goes active. MPX directly controls the LS257 latch so only need to write out
// and 8 bit value prior to RTSN going inactive.
// Normally the keyboard is in STROBE ALL mode. When a key is pressed, it commences a scan and when it arrives at the pressed
// key, RTSN cycle is suspended for varying amounts of time (ie 500us or 19ms) as the controller is looking for debounce and repeat.
//
// WARNING: The GPIO's are configurable via menuconfig BUT it is assumed all except RTSNi
//          are in the first GPIO bank and RTSNi is in the second GPIO bank. Modify the code
//          if RTSNi is set in the first bank or KDB[3:0], KDI4 are in the second bank.
//
#if defined(CONFIG_MODEL_MZ2800)
    IRAM_ATTR void mz28Interface( void * pvParameters )
    {
        // Locals.
        volatile uint32_t gpioIN;
        volatile uint8_t  strobeRow = 1;
    
        // Mask values declared as variables, let the optimiser decide wether they are constants or placed in-memory.
        uint32_t          rowBitMask = (1 << CONFIG_MZ_KDB3) | (1 << CONFIG_MZ_KDB2) | (1 << CONFIG_MZ_KDB1) | (1 << CONFIG_MZ_KDB0);
        uint32_t          colBitMask = (1 << CONFIG_MZ_KDO7) | (1 << CONFIG_MZ_KDO6) | (1 << CONFIG_MZ_KDO5) | (1 << CONFIG_MZ_KDO4) | 
                                       (1 << CONFIG_MZ_KDO3) | (1 << CONFIG_MZ_KDO2) | (1 << CONFIG_MZ_KDO1) | (1 << CONFIG_MZ_KDO0);
        uint32_t          KDB3_MASK  = (1 << CONFIG_MZ_KDB3);
        uint32_t          KDB2_MASK  = (1 << CONFIG_MZ_KDB2);
        uint32_t          KDB1_MASK  = (1 << CONFIG_MZ_KDB1);
        uint32_t          KDB0_MASK  = (1 << CONFIG_MZ_KDB0);
        uint32_t          KDI4_MASK  = (1 << CONFIG_MZ_KDI4);
        uint32_t          RTSNI_MASK = (1 << (CONFIG_MZ_RTSNI - 32));
    
        ESP_LOGI(MAINTAG, "Starting mz28Interface thread, colBitMask=%08x, rowBitMask=%08x.", colBitMask, rowBitMask);
    
        // Create, initialise and hold a spinlock so the current core is bound to this one method.
        portENTER_CRITICAL(&mzMutex);
    
        // Permanent loop, just wait for an RTSN strobe, latch the row, lookup matrix and output.
        for(;;)
        {
            #if defined(CONFIG_MZ_WIFI_ENABLED)
                // Whilst Wifi is active, suspend processing as we need to free up the core.
                if(wifiActivated)
                {
                    portEXIT_CRITICAL(&mzMutex);
                    while(wifiActivated);
                portENTER_CRITICAL(&mzMutex);
                }
            #endif
              
            // Detect RTSN going high, the MZ will send the required row during this cycle.
            if(REG_READ(GPIO_IN1_REG) & RTSNI_MASK)
            {
                // Slight delay needed as KD4 lags behind RTSN by approx 200ns and ROW number lags 850ns behind RTSN.
                for(volatile uint32_t delay=0; delay < 8; delay++);

                // Read the GPIO ports to get latest Row and KDI4 states.
                gpioIN = REG_READ(GPIO_IN_REG);
    
                // Assemble the required matrix row from the configured bits.
                strobeRow = ((gpioIN&KDB3_MASK) >> (CONFIG_MZ_KDB3-3)) | ((gpioIN&KDB2_MASK) >> (CONFIG_MZ_KDB2-2)) | ((gpioIN&KDB1_MASK) >> (CONFIG_MZ_KDB1-1)) | ((gpioIN&KDB0_MASK) >> CONFIG_MZ_KDB0);
             
                // Clear all KDO bits - clear state = '1'
                GPIO.out_w1ts = colBitMask;                                // Reset all scan data bits to '1', inactive.

                // Another short delay once the row has been assembled as we dont want to change the latch setting too soon, changing to soon leads to ghosting on previous row.
                for(volatile uint32_t delay=0; delay < 5; delay++);
    
                // KDI4 indicates if row data is needed or a single byte ANDing all the keys together, ie. to detect a key press without strobing all rows.
                if(gpioIN & KDI4_MASK)
                {
                    // Set all required KDO bits according to keyMatrix, set state = '0'.
                    GPIO.out_w1tc = mzControl.keyMatrixAsGPIO[strobeRow];  // Set to '0' active bits.
                } else
                {
                    // Set all required KDO bits according to the strobe all value. set state = '0'.
                    GPIO.out_w1tc = mzControl.strobeAllAsGPIO;             // Set to '0' active bits.
                }
    
                // Wait for RTSN to go low. No lockup guarding as timing is critical also the watchdog is disabled, if RTSN never goes low then the user has probably unplugged the interface!
                while(REG_READ(GPIO_IN1_REG) & RTSNI_MASK);
            }
    
            // Logic to feed the watchdog if needed. Watchdog disabled in menuconfig but if enabled this will need to be used.
            //TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
            //TIMERG0.wdt_feed=1;                       // feed dog
            //TIMERG0.wdt_wprotect=0;                   // write protect
            //TIMERG1.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
            //TIMERG1.wdt_feed=1;                       // feed dog
            //TIMERG1.wdt_wprotect=0;                   // write protect
        }
    }
#endif

// Method to refresh the transposed matrix used in the MZ interface. The normal key matrix is transposed to save valuable time
// because even though a core is dedicated to the MZ interface the timing is critical and the ESP-32 doesnt have spare horse power!
//
IRAM_ATTR void updateMirrorMatrix(void)
{
    // Locals.

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
    for(int idx2=0; idx2 < 15; idx2++)
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

    return;
}

// Method to convert the PS2 scan code into a key matrix representation which the MZ-2500/2800 is expecting.
//
IRAM_ATTR unsigned char updateMatrix(uint16_t data)
{
    // Locals.
    uint8_t   idx;
    uint8_t   changed = 0;
    uint8_t   matchExact = 0;

    // Loop through the entire conversion table to find a match on this key, if found appy the conversion to the virtual
    // switch matrix.
    //
    for(idx=0, changed=0, matchExact=0; idx < NUMELEM(PS2toMZ) && (changed == 0 || (changed == 1 && matchExact == 0)); idx++)
    {
        // Match key code?
        if(PS2toMZ[idx][PSMZTBL_KEYPOS] == (uint8_t)(data&0xFF) && ((PS2toMZ[idx][PSMZTBL_MACHINE] == MZ_ALL) || (PS2toMZ[idx][PSMZTBL_MACHINE] == mzControl.activeKeyMap)))
        {
            // Match Raw, Shift, Function, Control, ALT or ALT-Gr?
            if( (PS2toMZ[idx][PSMZTBL_SHIFTPOS] == 0 && PS2toMZ[idx][PSMZTBL_FUNCPOS] == 0 && PS2toMZ[idx][PSMZTBL_CTRLPOS] == 0 && PS2toMZ[idx][PSMZTBL_ALTPOS] == 0 && PS2toMZ[idx][PSMZTBL_ALTGRPOS] == 0) ||
                ((data & PS2_SHIFT)    && PS2toMZ[idx][PSMZTBL_SHIFTPOS] == 1) || 
                ((data & PS2_CTRL)     && PS2toMZ[idx][PSMZTBL_CTRLPOS]  == 1) ||
                ((data & PS2_ALT)      && PS2toMZ[idx][PSMZTBL_ALTPOS]   == 1) ||
                ((data & PS2_ALT_GR)   && PS2toMZ[idx][PSMZTBL_ALTGRPOS] == 1) ||
                ((data & PS2_GUI)      && PS2toMZ[idx][PSMZTBL_SHIFTPOS] == 1) || 
                ((data & PS2_FUNCTION) && PS2toMZ[idx][PSMZTBL_FUNCPOS]  == 1) )
            {
                
                // Exact entry match, data + control key? On an exact match we only process the first key. On a data only match we fall through to include additional data and control key matches to allow for un-mapped key combinations, ie. Japanese characters.
                matchExact = ((data & PS2_SHIFT)    && PS2toMZ[idx][PSMZTBL_SHIFTPOS] == 1) || 
                             ((data & PS2_CTRL)     && PS2toMZ[idx][PSMZTBL_CTRLPOS]  == 1) ||
                             ((data & PS2_ALT_GR)   && PS2toMZ[idx][PSMZTBL_ALTGRPOS] == 1) ||
                             ((data & PS2_ALT)      && PS2toMZ[idx][PSMZTBL_ALTPOS]   == 1) ||
                             ((data & PS2_GUI)      && PS2toMZ[idx][PSMZTBL_ALTPOS]   == 1) ||
                             ((data & PS2_FUNCTION) && PS2toMZ[idx][PSMZTBL_FUNCPOS]  == 1) ? 1 : 0;

                // RELEASE (PS2_BREAK == 1) or PRESS?
                if((data & PS2_BREAK))
                {
                    // Special case for the PAUSE / BREAK key. The underlying logic has been modified to send a BREAK key event immediately 
                    // after a PAUSE make, this is necessary as the Sharp MZ machines require SHIFT (pause) BREAK so the PS/2 CTRL+BREAK wont
                    // work (unless logic is added to insert a SHIFT, pause, add BREAK). The solution was to generate a BREAK event
                    // and add a slight delay for the key matrix to register it.
                    if((data&0x00FF) == PS2_KEY_PAUSE)
                    {
                        vTaskDelay(100);
                    }

                    // Loop through all the row/column combinations and if valid, apply to the matrix.
                    for(int row=PSMZTBL_MK_ROW1; row < PSMZTBL_MK_ROW3+1; row+=2)
                    {
                        // Reset the matrix bit according to the lookup table. 1 = No key, 0 = key in the matrix.
                        if(PS2toMZ[idx][row] != 0xFF)
                        {
                            mzControl.keyMatrix[PS2toMZ[idx][row]] |= PS2toMZ[idx][row+1];
                            changed = 1;
                        }
                    }
                   
                    // Loop through all the key releases associated with this key and reset the relevant matrix bit which was cleared on 
                    // initial keydown.
                    //
                    for(int row=PSMZTBL_BRK_ROW1; row < PSMZTBL_BRK_ROW2+1; row+=2)
                    {
                        if(PS2toMZ[idx][row] != 0xFF)
                        {
                            mzControl.keyMatrix[PS2toMZ[idx][row]] &= ~PS2toMZ[idx][row+1];
                            changed = 1;
                        }
                    }
                } else
                {
                    // Loop through all the key releases associated with this key and clear the relevant matrix bit.
                    // This is done first so as to avoid false key detection in the MZ logic.
                    //
                    for(int row=PSMZTBL_BRK_ROW1; row < PSMZTBL_BRK_ROW2+1; row+=2)
                    {
                        if(PS2toMZ[idx][row] != 0xFF)
                        {
                            mzControl.keyMatrix[PS2toMZ[idx][row]] |= PS2toMZ[idx][row+1];
                            changed = 1;
                        }
                    }
                   
                    // If a release key has been actioned, update the matrix and insert a slight pause to avoid
                    // the MZ logic seeing the released keys in combination with the newly pressed keys.
                    if(changed)
                    {
                        updateMirrorMatrix();
                        changed = 0;
                        vTaskDelay(10);
                    }

                    // Loop through all the row/column combinations and if valid, apply to the matrix.
                    for(int row=PSMZTBL_MK_ROW1; row < PSMZTBL_MK_ROW3+1; row+=2)
                    {
                        // Set the matrix bit according to the lookup table. 1 = No key, 0 = key in the matrix.
                        if(PS2toMZ[idx][row] != 0xFF)
                        {
                            mzControl.keyMatrix[PS2toMZ[idx][row]] &= ~PS2toMZ[idx][row+1];
                            changed = 1;
                        }
                    }
                }

                // Only spend time updating signals if an actual change occurred. Some keys arent valid so no change will be effected.
                if(changed)
                {
                    updateMirrorMatrix();
                }
            } // match key or a special function
        } // match key code
    } // for loop

    // Return flag to indicate if a match occurred and the matrix updated.
    return(changed);
}

// Primary PS/2 thread, running on Core 1.
// This thread is responsible for receiving PS/2 scan codes and mapping them to an MZ-2500/2800 keyboard matrix.
// The PS/2 data is received via interrupt.
//
IRAM_ATTR void ps2Interface( void * pvParameters )
{
    // Locals.
    uint16_t            scanCode      = 0x0000;
    bool                activityLED   = 0;
    TickType_t          ps2CheckTimer = 0;
    TickType_t          ps2LedTimer   = 0;
    bool                ps2Active     = 0;   // Flag to indicate the PS/2 keyboard is connected and online.
    #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
      uint8_t           dataChange    = 0;
      static int        scanPrtCol    = 0;
      static uint32_t   rfshTimer     = 0;
    #endif

    // Thread never exits, just polls the keyboard and updates the matrix.
    while(1)
    { 
        // Check the keyboard is online, this is done at startup and periodically to cater for user disconnect.
        if((xTaskGetTickCount() - ps2CheckTimer) > 1000 && (Keyboard.keyAvailable() == 0 || ps2Active == 0))
        {
            // Check to see if the keyboard is still available, no keyboard = no point!!
            // Firstly, ping keyboard to see if it is there.
            Keyboard.echo();              
            vTaskDelay(6);
            scanCode = Keyboard.read();
           
            // If the keyboard doesnt answer back, then it has been disconnected.
            if( (scanCode & 0xFF) != PS2_KEY_ECHO && (scanCode & 0xFF) != PS2_KEY_BAT)
            {
                // Re-initialise the subsystem, if the keyboard is plugged in then it will be detected on next loop.
                Keyboard.begin(CONFIG_PS2_HW_DATAPIN, CONFIG_PS2_HW_CLKPIN);

                // First entry print out message that the keyboard has disconnected.
                if(ps2Active == 1 || ps2CheckTimer == 0)
                {
                    ESP_LOGE(MAINTAG, "No PS2 keyboard detected, please connect.\n");
                    #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
                        ssd1306_display_text(&SSD1306, 0, (char *)"No PS2 Keyboard", 15, false);
                    #endif
                }
                ps2Active = 0;

                // Turn on LED when keyboard is detached.
                gpio_set_level((gpio_num_t)CONFIG_PWRLED, 1);
            } else
            {
                // First entry after keyboard starts responding, print out message.
                if(ps2Active == 0)
                {
                    ESP_LOGI(MAINTAG, "PS2 keyboard detected and online.\n");
                    ps2Active = 1;

                    // Flash LED to indicate Keyboard recognised.
                    gpio_set_level((gpio_num_t)CONFIG_PWRLED, 1);
                    ps2LedTimer = xTaskGetTickCount() - 400;
                }
            }
            ps2CheckTimer = xTaskGetTickCount(); // Check every second.
        } else
        {
            // Check for PS/2 keyboard scan codes.
            while((scanCode = Keyboard.read()) != 0)
            {
                #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
                    // Output the scan code for verification.
                    dbgprintf("%04x,", scanCode);
                    if(scanPrtCol++ >= 3) scanPrtCol = 0;
                #else
                    dbgprintf("%04x\n", scanCode);
                #endif

                // Filter out ALT+F1..3 keys as these select the active keymap.
                switch(scanCode)
                {
                    case 0x0961:
                        mzControl.activeKeyMap = MZ_2500;
                        break;
                    case 0x0962:
                        mzControl.activeKeyMap = MZ_2000;
                        break;
                    case 0x0963:
                        mzControl.activeKeyMap = MZ_80B;
                        break;

                    default:
                        // Update the virtual matrix with the new key value.
                        dataChange |= updateMatrix(scanCode);
                        break;
                }

                // Toggle LED to indicate data flow.
                gpio_set_level((gpio_num_t)CONFIG_PWRLED, activityLED);
                activityLED = !activityLED;

                // Reset the check keyboard timer, no need to check as activity is seen.
                ps2CheckTimer = xTaskGetTickCount(); // Check every second.
                ps2LedTimer = xTaskGetTickCount();
            }

            // If no activity has been seen for 100ms then switch off the LED.
            if(ps2LedTimer > 0 && (xTaskGetTickCount() - ps2LedTimer) > 100)
            {
                activityLED = 0;
                ps2LedTimer = 0;
                gpio_set_level((gpio_num_t)CONFIG_PWRLED, activityLED);
            }

        #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
            if(dataChange || (rfshTimer > 0 && --rfshTimer == 0))
            {
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

                // Clear timer for next refresh.
                rfshTimer = 2000000;
                dataChange = 0;
            }
        #endif
        }

        // Let other tasks run.
        vTaskDelay(10);
   }
}

#if defined(CONFIG_MZ_WIFI_ENABLED)
// Event handler for Client mode Wifi event callback.
//
IRAM_ATTR void wifiClientHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // Locals.
    //
   
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        if(clientRetryCnt < CONFIG_MZ_WIFI_MAX_RETRIES)
        {
            esp_wifi_connect();
            clientRetryCnt++;
            ESP_LOGI(WIFITAG, "retry to connect to the AP");
        } else 
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFITAG,"connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFITAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        clientRetryCnt = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    return;
}

// Event handler for Access Point mode Wifi event callback.
//
IRAM_ATTR void wifiAPHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // Locals.
    //


    if (event_id == WIFI_EVENT_AP_STACONNECTED) 
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFITAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFITAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
    return;
}

// Method to initialise the interface as a client to a known network, the SSID
// and password have already been setup.
//
uint8_t setupWifiClient(void)
{
    // Locals.
    //
    wifi_init_config_t           wifiInitConfig;
    esp_event_handler_instance_t instID;
    esp_event_handler_instance_t instIP;
    EventBits_t                  bits;
    wifi_config_t                wifiConfig = { .sta =  {
                                     /* ssid            */ CONFIG_MZ_SSID,
                                     /* password        */ CONFIG_MZ_DEFAULT_SSID_PWD,
                                     /* scan_method     */ {},
                                     /* bssid_set       */ {},
                                     /* bssid           */ {},
                                     /* channel         */ {},
                                     /* listen_interval */ {},
                                     /* sort_method     */ {},
                                     /* threshold       */ {
                                     /* rssi            */     {},
                                     /* authmode        */     WIFI_AUTH_WPA2_PSK
                                                           },
                                     /* pmf_cfg         */ {
                                     /* capable         */     true,
                                     /* required        */     false
                                                           },
                                     /* rm_enabled      */ {},
                                     /* btm_enabled     */ {},
                                     /* mbo_enabled     */ {}, // For IDF 4.4 and higher
                                     /* reserved        */ {}
                                                        }
                                              };
   
    // Create an event handler group to manage callbacks.
    s_wifi_event_group = xEventGroupCreate();

    // Setup the network interface.
    if(esp_netif_init())
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise netif, disabling WiFi.");
        return(1);
    }

    // Setup the event loop.
    if(esp_event_loop_create_default())
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise event loop, disabling WiFi.");
        return(1);
    }

    // Setup the wifi client (station).
    esp_netif_create_default_wifi_sta();

    // Setup the config for wifi.
    wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&wifiInitConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise wifi with default parameters, disabling WiFi.");
        return(1);
    }

    // Register event handlers.
    if(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiClientHandler, NULL, &instID))
    {
        ESP_LOGI(WIFITAG, "Couldnt register event handler for ID, disabling WiFi.");
        return(1);
    }
    if(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiClientHandler, NULL, &instIP)) 
    {
        ESP_LOGI(WIFITAG, "Couldnt register event handler for IP, disabling WiFi.");
        return(1);
    }
    if(esp_wifi_set_mode(WIFI_MODE_STA))
    {
        ESP_LOGI(WIFITAG, "Couldnt set Wifi mode to Client, disabling WiFi.");
        return(1);
    }
    if(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt configure client mode, disabling WiFi.");
        return(1);
    }
    if(esp_wifi_start())
    {
        ESP_LOGI(WIFITAG, "Couldnt start Client session, disabling WiFi.");
        return(1);
    }

    // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    // number of re-tries (WIFI_FAIL_BIT). The bits are set by wifiClientHandler() (see above)
    bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
    // happened.
    if(bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(WIFITAG, "Connected: SSID:%s password:%s", CONFIG_MZ_SSID, CONFIG_MZ_DEFAULT_SSID_PWD);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(WIFITAG, "Connection Fail: SSID:%s, password:%s", CONFIG_MZ_SSID, CONFIG_MZ_DEFAULT_SSID_PWD);
    } 
    else
    {
        ESP_LOGE(WIFITAG, "Unknown evemt, bits:%d", bits);
    }

    // Close connection, not yet ready with application.
    if(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instIP))
    {
        ESP_LOGI(WIFITAG, "Couldnt unregister IP assignment halder, disabling WiFi.");
        return(1);
    }
    if(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instID))
    {
        ESP_LOGI(WIFITAG, "Couldnt unregister ID assignment halder, disabling WiFi.");
        return(1);
    }
    vEventGroupDelete(s_wifi_event_group);

    // No errors.
    return(0);
}

// Method to initialise the interface as a Soft Access point with a given SSID
// and password.
// The Access Point mode is basically to bootstrap a Client connection where the 
// client connecting provides the credentials in order to connect as a client to 
// another AP to join a local network.
//
uint8_t setupWifiAP(void)
{
    // Locals.
    //
    esp_err_t                    retcode;
    wifi_init_config_t           wifiInitConfig;
    wifi_config_t                wifiConfig = { .ap =  {
                                     /* ssid            */ CONFIG_MZ_SSID,
                                     /* password        */ CONFIG_MZ_DEFAULT_SSID_PWD,
                                     /* ssid_len        */ strlen(CONFIG_MZ_SSID),
                                     /* channel         */ CONFIG_MZ_WIFI_AP_CHANNEL,
                                     /* authmode        */ WIFI_AUTH_WPA_WPA2_PSK,
                                     /* hidden          */ CONFIG_MZ_WIFI_SSID_HIDDEN,
                                     /* nax_connection  */ CONFIG_MZ_WIFI_MAX_CONNECTIONS,
                                     /* beacon_interval */ 100,
                                     /* pairwise_cipher */ WIFI_CIPHER_TYPE_TKIP,
                                     /* ftm_responder   */ 0,
                                  // /* pmf_cfg         */ {
                                  // /* capable         */     true,
                                  // /* required        */     false
                                  //                       }
                                                       }
                                              };

    // Intialise the network interface.
    if(esp_netif_init())
    {
        ESP_LOGI(WIFITAG, "Couldnt initialise network interface, disabling WiFi.");
        return(1);
    }
    if((retcode = esp_event_loop_create_default()))
    {
        ESP_LOGI(WIFITAG, "Couldnt create default loop(%d), disabling WiFi.", retcode);
        return(1);
    }

    // Create the default Access Point.
    //
    esp_netif_create_default_wifi_ap();

    // Initialise AP with default parameters.
    wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&wifiInitConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt setup AP with default parameters, disabling WiFi.");
        return(1);
    }

    // Setup callback handlers for wifi events.
    if(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiAPHandler, NULL, NULL))
    {
        ESP_LOGI(WIFITAG, "Couldnt setup event handlers, disabling WiFi.");
        return(1);
    }

    // If there is no password for the access point set authentication to open.
    if (strlen(CONFIG_MZ_DEFAULT_SSID_PWD) == 0) 
    {
        wifiConfig.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Setup as an Access Point.
    if(esp_wifi_set_mode(WIFI_MODE_AP))
    {
        ESP_LOGI(WIFITAG, "Couldnt set mode to Access Point, disabling WiFi.");
        return(1);
    }
    // Configure the Access Point
    if(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig))
    {
        ESP_LOGI(WIFITAG, "Couldnt configure Access Point, disabling WiFi.");
        return(1);
    }
    // Start the Access Point.
    if(esp_wifi_start())
    {
        ESP_LOGI(WIFITAG, "Couldnt start Access Point session, disabling WiFi.");
        return(1);
    }

    // No errors.
    return(0);
}
#endif

// Work in progress. Intention is to add a WiFi access point to garner local net details,
// connect to local net then offer a browser interface to change keys.
#if defined(CONFIG_MZ_WIFI_ENABLED)
IRAM_ATTR void wifiInterface( void * pvParameters )
{
    // Locals.
    uint32_t          keyDebCtr     = 0;
    uint32_t          WIFIEN_MASK   = (1 << (CONFIG_MZ_WIFI_EN_KEY - 32));
    esp_err_t         nvsStatus;
    enum WIFIMODES    wifiMode = WIFI_OFF;
   

    // Loop forever, detecting the Wifi activation/de-activation and subsequent processing.
    while(1)
    { 
        // Has wifi been activated? If the Wifi switch has been pressed, initialise Wifi according to desired mode.
        //
        if(wifiMode != WIFI_OFF)
        {
            if(!wifiActivated)
            {
                // Initialise the NVS storage, needed for WiFi parameters.
                nvsStatus = nvs_flash_init();
                if (nvsStatus == ESP_ERR_NVS_NO_FREE_PAGES || nvsStatus == ESP_ERR_NVS_NEW_VERSION_FOUND)
                {
                    ESP_ERROR_CHECK(nvs_flash_erase());
                    nvsStatus = nvs_flash_init();
                }

                if(nvsStatus)
                {
                    ESP_LOGI(WIFITAG, "Couldnt initialise NVS, disabling WiFi.");
                    wifiActivated = 0;
                    wifiMode = WIFI_OFF;
                } else
                {
                    if(wifiMode == WIFI_ON)
                    {
                        if(setupWifiClient())
                        {
                            wifiActivated = 0;
                            wifiMode = WIFI_OFF;
                        } else
                        {
                            dbgprintf("Wifi Client %s\n", wifiActivated ? "activated" : "de-activated");
                            wifiActivated = 1;
                        }
                    }
                    else if(wifiMode == WIFI_CONFIG_AP)
                    {
                        if(setupWifiAP())
                        {
                            wifiActivated = 0;
                            wifiMode = WIFI_OFF;
                        } else
                        {
                            dbgprintf("Wifi AP %s\n", wifiActivated ? "activated" : "de-activated");
                            wifiActivated = 1;
                        }
                    }
                }

                // Re-init switch variables for next activation.
                keyDebCtr = 0;
            }

        }
       
        // Check the switch, has it gone to zero, ie. pressed?
        //
        if((REG_READ(GPIO_IN1_REG) & WIFIEN_MASK) == 0)
        {
            // On first press, wait 1 second to see if user is selecting WiFi on or WiFi Config.
            if(keyDebCtr == 0)
            {
                wifiMode = WIFI_OFF;
                keyDebCtr = 10;
            }
            // If counter gets to 1 then assume WiFi on.
            else if(keyDebCtr == 1)
            {
                wifiMode = WIFI_ON;
                // Reset counter for 10 seconds, 10 being required to enter WiFi Config AP mode.
                keyDebCtr = 100;
            }
            // 9 seconds later, mode is Wifi Config AP.
            else if(keyDebCtr == 11)
            {
                wifiMode = WIFI_CONFIG_AP;
            } 
            else if(keyDebCtr > 0)
            {
                keyDebCtr--;
            }
        }

        // Let other tasks run. NB. This value affects the debounce counter, update as necessary.
        vTaskDelay(100);
   }
}
#endif

// Setup method to configure ports, devices and threads prior to application run.
// Configuration:
//      PS/2 Keyboard over 2 wire interface
//      Power/Status LED
//      Optional OLED debug output screen
//      4 bit input  - MZ-2500/2800 Row Number
//      8 bit output - MZ-2500/2800 Scan data
//      1 bit input  - RTSN strobe line, low indicating a new Row Number available.
//      1 bit input  - KD4, High = Key scan data required, Low = AND of all key matrix rows required.
//
void setup()
{
    // Locals.
    gpio_config_t io_conf;
   
    // Setup power LED first to show life.
    ESP_LOGI(MAINTAG, "Configuring Power LED.");
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT; 
    io_conf.pin_bit_mask = (1ULL<<CONFIG_PWRLED); 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)CONFIG_PWRLED, 1);

    // Start the keyboard, no mouse.
    ESP_LOGI(MAINTAG, "Initialise PS2 keyboard.");
    Keyboard.begin(CONFIG_PS2_HW_DATAPIN, CONFIG_PS2_HW_CLKPIN);

    // If OLED connected and enabled, include the screen controller for debug output.
    //
    #if defined(CONFIG_DEBUG_OLED) || !defined(CONFIG_OLED_DISABLED)
        #if CONFIG_I2C_INTERFACE
            ESP_LOGI(MAINTAG, "INTERFACE is i2c");
            ESP_LOGI(MAINTAG, "CONFIG_SDA_GPIO=%d",     CONFIG_SDA_GPIO);
            ESP_LOGI(MAINTAG, "CONFIG_SCL_GPIO=%d",     CONFIG_SCL_GPIO);
            ESP_LOGI(MAINTAG, "CONFIG_RESET_GPIO=%d",   CONFIG_RESET_GPIO);
            i2c_master_init(&SSD1306, CONFIG_SDA_GPIO,  CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
        #endif // CONFIG_I2C_INTERFACE
        #if CONFIG_SPI_INTERFACE
            ESP_LOGI(MAINTAG, "INTERFACE is SPI");
            ESP_LOGI(MAINTAG, "CONFIG_MOSI_GPIO=%d",    CONFIG_MOSI_GPIO);
            ESP_LOGI(MAINTAG, "CONFIG_SCLK_GPIO=%d",    CONFIG_SCLK_GPIO);
            ESP_LOGI(MAINTAG, "CONFIG_CS_GPIO=%d",      CONFIG_CS_GPIO);
            ESP_LOGI(MAINTAG, "CONFIG_DC_GPIO=%d",      CONFIG_DC_GPIO);
            ESP_LOGI(MAINTAG, "CONFIG_RESET_GPIO=%d",   CONFIG_RESET_GPIO);
            spi_master_init(&SSD1306, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
        #endif // CONFIG_SPI_INTERFACE

        #if CONFIG_SSD1306_128x64
            ESP_LOGI(MAINTAG, "Panel is 128x64");
            ssd1306_init(&SSD1306, 128, 64);
        #endif // CONFIG_SSD1306_128x64
        #if CONFIG_SSD1306_128x32
            ESP_LOGI(MAINTAG, "Panel is 128x32");
            ssd1306_init(&SSD1306, 128, 32);
        #endif // CONFIG_SSD1306_128x32

        ssd1306_clear_screen(&SSD1306, false);
        ssd1306_contrast(&SSD1306, 0xff);
    #endif

    // Configure 4 inputs to be the Strobe Row Number which is used to index the virtual key matrix and the strobe data returned.
    #if !defined(CONFIG_MZ_DISABLE_KDB)
        ESP_LOGI(MAINTAG, "Configuring MZ-2500/2800 4 bit Row Number Inputs.");
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
        ESP_LOGI(MAINTAG, "Configuring MZ-2500/2800 8 bit Strobe data Outputs.");
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
        ESP_LOGI(MAINTAG, "Configuring MZ-2500/2800 RTSN Input.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_RTSNI); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    #if !defined(CONFIG_MZ_DISABLE_RTSNI)
        ESP_LOGI(MAINTAG, "Configuring MZ-2500/2800 KD4 Input.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_KDI4); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    #if defined(CONFIG_MZ_WIFI_ENABLED)
        ESP_LOGI(MAINTAG, "Configuring WiFi Enable Switch.");
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT; 
        io_conf.pin_bit_mask = (1ULL<<CONFIG_MZ_WIFI_EN_KEY); 
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    #endif

    // Create a task pinned to core 0 which will fulfill the MZ-2500/2800 interface. This task has the highest priority
    // and it will also hold spinlock and manipulate the watchdog to ensure a scan cycle timing can be met. This means 
    // all other tasks running on Core 1 will suspend. The PS/2 controller, running on the ULP processor will continue
    // to interact with the PS/2 keyboard and buffer scan codes.
    //
    // Core 1 - MZ Interface
    #if defined(CONFIG_MODEL_MZ2500)
        ESP_LOGI(MAINTAG, "Starting mz25if thread...");
        xTaskCreatePinnedToCore(mz25Interface, "mz25if", 32768, NULL, 25, &TaskMZ25IF, 1);
    #endif
    #if defined(CONFIG_MODEL_MZ2800)
        ESP_LOGI(MAINTAG, "Starting mz28if thread...");
        xTaskCreatePinnedToCore(mz28Interface, "mz28if", 32768, NULL, 25, &TaskMZ28IF, 1);
    #endif
    vTaskDelay(500);

    // Core 0 - Application
    // PS/2 Interface handler thread.
    ESP_LOGI(MAINTAG, "Starting ps2if thread...");
    xTaskCreatePinnedToCore(ps2Interface, "ps2if",   32768, NULL, 22, &TaskPS2IF, 0);
    vTaskDelay(500);

    // Core 9 - WiFi handler thread.
    #if defined(CONFIG_MZ_WIFI_ENABLED)
        ESP_LOGI(MAINTAG, "Starting wifi thread...");
        xTaskCreatePinnedToCore(wifiInterface, "wifi", 32768, NULL, 1, &TaskWIFI,  0);
        vTaskDelay(500);
    #endif
}

// ESP-IDF Application entry point.
//
extern "C" void app_main()
{
    // Setup hardware and start primary control threads,
    setup();

    // Lost in space.... this thread is no longer required!
}
