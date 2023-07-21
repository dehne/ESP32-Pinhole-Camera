/****
 * ESP32 Pinhole Camera v0.5.0
 * 
 * main.cpp
 * 
 * This is the Arduino framework-base firmware for a pinhole camera based on the Thinker AI
 * ESP32 CAM board. The idea is to use the ESP32 CAM board and camera as a "point and shoot" 
 * camera that saves the pictures it takes to an SD card inserted into the SD card slot 
 * built into the ESP32 CAM board. It's called a "pinhole camera" because the impetus for 
 * building it was to make a simple digital pinhole camera by substituting a pinhole for the 
 * camera lens. Just because.
 * 
 * The only controls are a switch that acts as the shutter release and the reset button. To 
 * use the camera, power up the ESP32 and wait for the built-in red LED to flash five times. 
 * (There are two built-in LEDS. The red one is marked LED1 and is on the side of the board 
 * with the processor. The other is the white "flash" LED on the camera side.) Once the red 
 * LED flashes, the camera is ready. If it repeatedly flashes, pauses and flashes again, 
 * count the number of flashes in a group to figure out what went wrong:
 * 
 *    Flashes  Problem
 *    =======  ====================================
 *          2  Camera initialization failed
 *          3  SD card file system mount failed
 *          4  No SD Card found in the card reader
 * 
 * To use the camera, click its shutter. The red LED will flash once to indicate that the image 
 * was captured and saved. If there's no flash, something went wrong. Maybe I'll add more error 
 * indicator LED flashing if this turns out to be a problem.
 * 
 * Activity on the SD card occurs at two only points. First, during initialization. And, second, 
 * after the shutter is pressed and before the red LED flashes to indicate the image was captured. 
 * So, it should be okay to pull the power on the camera at other times.
 * 
 * If the shutter isn't clicked for five minutes the camera will flash the red LED five times 
 * and go into deep sleep mode. To get it going again, press the reset button on the board.
 * 
 * A few technical notes
 * =====================
 * 
 * The ESP32 CAM board has very few (i.e., basically no) exposed unused GPIO pins if both the 
 * camera and SD card reader are being used in the default way. If you try to use a GPIO that's 
 * used by one of the libraries, stuff (unsurprisingly) breaks. Most commonly this shows up as 
 * an unhandled exception that triggers a builtin thing called the "guru" to do a mini-dump. 
 * 
 * I needed a GPIO to use for the shutter release. The documentation shows that GPIO 16 should 
 * be free, but trying to use it causes a crash. I didn't try very hard to figure out what was 
 * using it since whatever it was obviously needed the GPIO. So I tried the other exposed GPIOs. 
 * All the ones I tried caused a crash. So I looked for another way forward.
 * 
 * By default the SD card library makes a 4-bit data bus of four of the exposed GPIOs, but it 
 * also supports doing data transfers (more slowly) using a single GPIO, thus freeing up three of 
 * the exposed GPIOs. One of the ones freed up is GPIO 4, which is the one to which the white 
 * "camera flash" LED is attached. That's why the white LED flashes and glows by when the SD card 
 * is used. Anyway, I forced the SD card library to use one wire (SD_MMC.begin("/sdcard", true)). 
 * That worked fine, is plenty fast for what I need, stopped the white LED from flashing and 
 * glowing, and freed up GPIO 12 for me to use for the shutter switch. 
 *  
 ****
 *
 * Copyright 2023 by D.L. Ehnebuske
 * License: GNU Lesser General Public License v2.1
 * 
 ****/
#include "Arduino.h"                              // Arduino framework
#include "esp_camera.h"                           // Camera support
#include "FS.h"                                   // File system
#include "SD_MMC.h"                               // SD Card support
#include "soc/soc.h"                              // Disable brownout checking
#include "soc/rtc_cntl_reg.h"                     // Disable brownout checking
#include "driver/rtc_io.h"                        // RTC GPIO hold functions
#include <EEPROM.h>                               // EEPROM access
#include <PushButton.h>                           // Simple push button

// Uncomment to enable rather verbose debug printing
//#define DEBUG

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     (32)
#define RESET_GPIO_NUM    (-1)
#define XCLK_GPIO_NUM     (0)
#define SIOD_GPIO_NUM     (26)
#define SIOC_GPIO_NUM     (27)
#define Y9_GPIO_NUM       (35)
#define Y8_GPIO_NUM       (34)
#define Y7_GPIO_NUM       (39)
#define Y6_GPIO_NUM       (36)
#define Y5_GPIO_NUM       (21)
#define Y4_GPIO_NUM       (19)
#define Y3_GPIO_NUM       (18)
#define Y2_GPIO_NUM       (5)
#define VSYNC_GPIO_NUM    (25)
#define HREF_GPIO_NUM     (23)
#define PCLK_GPIO_NUM     (22)

// Misc compile-time definitions
#define BANNER            "\nESP32 CAM Pinhole camera v0.5.0\n"
#define IC_ADDR           (0)                       // Image counter address in "EEPROM"
#define SERIAL_MILLIS     (3000)                    // Maximum millis to wait for Serial to become ready
#define FLASH_MILLIS      (200)                     // LED_BUILTIN default flash length (millis())
#define FAIL_MILLIS       (1000)                    // millis() between flash groups for init failures
#define WAVE_FLASH_COUNT  (5)                       // Number of flashes to say hello/goodbye
#define SNAP_FLASH_COUNT  (1)                       // Number of times to flash on shutter release
#define CAMI_FLASH_COUNT  (2)                       // Number of times to flash if camera init fails
#define SDMI_FLASH_COUNT  (3)                       // Number of times to flash if SD card mount fails
#define SDCI_FLASH_COUNT  (4)                       // Number of times to flash if no SD card found
#define LED_BUILTIN       (GPIO_NUM_33)             // The GPIO for the little red LED (active LOW)
#define AWAKE_MILLIS      (300000UL)                // millis() to stay awake waiting for shutter press

// Global variables
PushButton shutter {GPIO_NUM_12};                   // The "shutter" switch
uint16_t imageCtr;                                  // The image counter for numbering image files

/**
 * @brief Flash the little red LED
 * 
 * @param flashCount Number of times to flash
 */
void flashBuiltinLed(uint8_t flashCount = WAVE_FLASH_COUNT, uint8_t flashLen = FLASH_MILLIS) {
    for (uint8_t i = 0; i < flashCount; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(flashLen);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i + 1 < flashCount) {
      delay(flashLen);
    }  
  }
}

/**
 * @brief Arduino setup function: Called once at power-on or reset
 * 
 */
void setup() {
  // Get Serial going
  Serial.begin(9600);
  Serial.print(BANNER);
  #ifdef DEBUG
  Serial.setDebugOutput(true);
  #endif

  // Initialize the builtin little red LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // It's active low

  // Set up the camera configuration we'll use
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  if(psramFound()){
    #ifdef DEBUG
    Serial.print("Using UXGA resolution.\n");
    #endif
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    #ifdef DEBUG
    Serial.print("Using SVGA resolution because PSRAM not present.\n");
    #endif
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Initialize the camera with the configuration we just set up
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x.\n", err);
    while (true) {
      flashBuiltinLed(CAMI_FLASH_COUNT);
      delay(FAIL_MILLIS);
    }
  }
  
  // Mount SD card
  if(!SD_MMC.begin("/sdcard", true)){
    Serial.print("SD Card Mount failed.\n");
    while (true) {
      flashBuiltinLed(SDMI_FLASH_COUNT);
      delay(FAIL_MILLIS);
    }
  }
  
  #ifdef DEBUG
  Serial.print("SD card mounted.\n");
  #endif
  
  // Verify there's a card in it
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.print("No SD Card inserted.\n");
    while (true) {
      flashBuiltinLed(SDCI_FLASH_COUNT);
      delay(FAIL_MILLIS);
    }
  }
  #ifdef DEBUG
  Serial.print("The SD card reader seems to have a card in it.\n");
  #endif
  
  // Get "EEPROM" going (it's really flash memory)
  EEPROM.begin(sizeof((uint16_t)0));

  // Uncomment to reset image counter in EEPROM to 0
  //EEPROM.writeUShort(IC_ADDR, (uint16_t)0);
  //EEPROM.commit();

  // Initialize the image counter
  imageCtr = EEPROM.readUShort(IC_ADDR);
  #ifdef DEBUG
  Serial.printf("Last stored image was Image%d.jpg.\n", imageCtr);
  #endif

  // Start the shutter switch
  shutter.begin();

  // Show we're ready
  flashBuiltinLed();
  #ifdef DEBUG
  Serial.print("Initialization complete.\n");
  #endif
}

/**
 * @brief Arduino loop() function. Called repeatedly after setup() completes.
 * 
 */
void loop() {
  static unsigned long clickedMillis = millis();                  // When the shutter was last clicked

  // Take a picture if the shutter was depressed
  if (shutter.clicked()) {
    clickedMillis = millis();

    // Capture image
    camera_fb_t * fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.print("Camera capture failed.\n");
      return;
    }
    #ifdef DEBUG
    Serial.print("Got the framebuffer.\n");
    #endif

    // Figure out what to call the image file
    String path = "/Image" + String(++imageCtr) + ".jpg";
    #ifdef DEBUG
    Serial.printf("The file name for the image is '%s'.\n", path.c_str());
    #endif

    // Save the image
    File file = SD_MMC.open(path.c_str(), FILE_WRITE);
    if(!file){
      Serial.print("Unable to create the file for the image.\n");
      return;
    }
    size_t sz = file.write(fb->buf, fb->len);
    Serial.printf("Saved image to: '%s' (%d bytes)\n", path.c_str(), fb->len);
    EEPROM.writeUShort(IC_ADDR, imageCtr);
    EEPROM.commit();

    flashBuiltinLed(SNAP_FLASH_COUNT);
    #ifdef DEBUG
    Serial.printf("Committed imageCtr (%d) to 'eeprom'.\n", imageCtr);
    #endif

    // Clean up
    file.close();
    esp_camera_fb_return(fb); 
  }

  // If it's been a long time since the shutter was clicked, go to sleep. (Press reset button to wake up.)
  if (millis() - clickedMillis > AWAKE_MILLIS) {
    // Shutdown "eeprom"
    EEPROM.end();

    // Sleepy-byes.
    Serial.print("Going to sleep.\n");
    flashBuiltinLed();
    esp_deep_sleep_start();
  }
}