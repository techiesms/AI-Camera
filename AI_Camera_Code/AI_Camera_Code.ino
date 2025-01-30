/* 

The Code was test with 

   Arduino 1.8.13
   ESP32 BOARD PACKAGE 1.0.6
   TJpg_Decoder By Bodmer 0.0.3
   TFT_SPI By Bodmer 2.3.4
   lvgl By kisvegabor 7.8.1

*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <Base64.h>
#include <lvgl.h>
#include "esp_camera.h"
#include <ArduinoJson.h>

const char* ssid = "SSID";
const char* password = "PASS";

const String apiKey = "YOUR GPT-4o API KEY";

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define FLASH_GPIO_NUM 4  // Flash pin (if using external flash)
#define BUZZER_PIN 3      // Buzzer connected to GPIO3 (for sound indication)
#define BUTTON_PIN 1      // Button pin for triggering capture (GPIO 1)


TFT_eSPI tft = TFT_eSPI();
static lv_disp_buf_t disp_buf;               // Buffer to hold pixel data for LVGL
static lv_color_t buf[LV_HOR_RES_MAX * 10];  // Buffer to store pixel data for LVGL

// Declare pointers for LVGL keyboard and text area objects
static lv_obj_t* kb;
static lv_obj_t* ta;

String userInputText = "";        // Holds the text entered by the user
String capturedImageBase64 = "";  // Holds the Base64-encoded image data

bool flashOn = false;              // Track flash state
bool isCaptureMode = false;        // Indicates if the system is in capture mode
bool isResponseDisplayed = false;  // Indicates if a response has been displayed

camera_fb_t* capturedFrameBuffer = nullptr;  // Holds the captured frame buffer


// Function to encode image to Base64
String encodeImageToBase64(uint8_t* imageData, size_t imageSize) {
  return base64::encode((const uint8_t*)imageData, imageSize);
}


void setup() {
  Serial.begin(115200);

  displayInit();
  drawBorders();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);  // Ensure flash is off initially

  displayLargeCenteredMessage("AI CAMERA BY TECHIESMS");
  delay(1000);

  WiFi.begin(ssid, password);


  displayLargeCenteredMessage("Connecting to WiFi...");
  delay(500);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("...");
  }
  Serial.println("WiFi Connected!");
  Serial.println("IP Address: " + WiFi.localIP().toString());
  displayLargeCenteredMessage("WiFi Connected!");
  delay(500);

  cameraInit();  // Initialize camera
  lvglInit();    // Initialize LVGL (LittlevGL) graphics library
  lv_layout();   // Initialize LVGL layout and interface
}


// Initializes the TFT display and sets up touch calibration and image decoder
void displayInit() {
  tft.begin();
  tft.setRotation(1);  // Set display rotation (1 is usually landscape mode)
  tft.fillScreen(TFT_BLACK);

  uint16_t calData[5] = { 292, 3562, 332, 3384, 7 };  // Calibration data for the touch screen
  tft.setTouch(calData);                              // Set the touch calibration data for the screen

  drawBorders();

  TJpgDec.setJpgScale(1);           // Set the scale of the JPEG image (1 means no scaling)
  TJpgDec.setSwapBytes(true);       // Set byte swapping to handle different color formats correctly
  TJpgDec.setCallback(tft_output);  // Set the callback function to draw the image on TFT display
}


// LVGL display flush callback function for rendering the display buffer on the screen
void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}


// Callback function used by the JPEG decoder to draw an image on the TFT screen
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}


// Captures and displays a camera feed on the TFT display
void displayCameraFeed() {
  camera_fb_t* fb = esp_camera_fb_get();  // Capture a frame from the camera
  if (!fb) {
    Serial.println("Camera Capture Failed: Frame buffer is NULL!");
    return;
  }

  // Check if the captured frame is in the correct format (JPEG)
  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("Camera Capture Failed: Incorrect format!");
    esp_camera_fb_return(fb);
    return;
  }

  // Draw the captured JPEG image on the screen using the JPEG decoder
  TJpgDec.drawJpg(80, 5, (const uint8_t*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  drawBorders();
}


void cameraInit() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 9000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    displayMessage("Camera Initialization Failed!");
    return;
  }
  displayLargeCenteredMessage("Camera Initialized!");
  delay(500);
  drawBorders();
}


void lvglInit() {
  lv_init();  // Initialize LVGL

  lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);  // Initialize the display buffer and driver

  lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 480;
  disp_drv.ver_res = 320;
  disp_drv.flush_cb = my_disp_flush;  // Custom flush function to update screen
  disp_drv.buffer = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  // Initialize touch input device driver
  lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;  // Custom touchpad read function
  lv_indev_drv_register(&indev_drv);

  // Initialize layout
  lv_layout();
}

void lv_layout() {
  // Create a material theme for consistency
  lv_theme_t* th = lv_theme_material_init(LV_THEME_DEFAULT_COLOR_PRIMARY, LV_THEME_DEFAULT_COLOR_SECONDARY, LV_THEME_MATERIAL_FLAG_DARK, LV_THEME_DEFAULT_FONT_SMALL, LV_THEME_DEFAULT_FONT_NORMAL, LV_THEME_DEFAULT_FONT_SUBTITLE, LV_THEME_DEFAULT_FONT_TITLE);

  lv_obj_t* scr = lv_obj_create(NULL, NULL);
  lv_scr_load(scr);

  // Flash button
  lv_obj_t* flashBtn = lv_btn_create(scr, NULL);
  lv_obj_set_pos(flashBtn, 412, 55);  // Adjust as needed
  lv_obj_set_size(flashBtn, 50, 50);
  lv_obj_set_event_cb(flashBtn, flash_btn_event_cb);
  lv_obj_t* flashLabel = lv_label_create(flashBtn, NULL);
  lv_label_set_text(flashLabel, "Flash");

  // Capture button
  lv_obj_t* captureBtn = lv_btn_create(scr, NULL);
  lv_obj_set_pos(captureBtn, 408, 125);  // Adjust as needed
  lv_obj_set_size(captureBtn, 70, 65);
  lv_obj_set_event_cb(captureBtn, capture_btn_event_cb);
  lv_obj_t* captureLabel = lv_label_create(captureBtn, NULL);
  lv_label_set_text(captureLabel, "Capture");

  // Bottom message label
  lv_obj_t* messageLabel = lv_label_create(scr, NULL);
  lv_label_set_text(messageLabel, "Press the button to capture a new image");
  // Position the label at the bottom
  lv_obj_align(messageLabel, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -30);
}


void flash_btn_event_cb(lv_obj_t* btn, lv_event_t event) {
  if (event == LV_EVENT_CLICKED) {
    flashOn = !flashOn;
    digitalWrite(FLASH_GPIO_NUM, flashOn ? HIGH : LOW);
    Serial.println(flashOn ? "Flash ON" : "Flash OFF");
  }
}


void capture_btn_event_cb(lv_obj_t* btn, lv_event_t event) {
  if (event == LV_EVENT_CLICKED) {
    Serial.println("Button clicked, capturing image...");
    displayLargeCenteredMessage("Capturing Image...");
    delay(100);
    captureAndProcessImage();
  }
}


void createKeyboard() {

  // Create a new screen for the keyboard and text area
  lv_obj_t* keyboard_screen = lv_obj_create(NULL, NULL);
  lv_scr_load(keyboard_screen);  // Load the new screen
  lv_obj_set_style_local_bg_opa(keyboard_screen, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_obj_set_style_local_bg_color(keyboard_screen, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);

  // Create the text area
  lv_obj_t* ta_question = lv_textarea_create(keyboard_screen, NULL);
  lv_obj_set_size(ta_question, 400, 100);
  lv_obj_align(ta_question, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
  lv_textarea_set_placeholder_text(ta_question, "Enter your question:");
  lv_textarea_set_text(ta_question, "");

  // Create the keyboard
  lv_obj_t* kb = lv_keyboard_create(keyboard_screen, NULL);
  lv_obj_set_size(kb, LV_HOR_RES, LV_VER_RES / 2);
  lv_obj_align(kb, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_cursor_manage(kb, true);

  // Apply keyboard styles
  static lv_style_t kb_style;
  lv_style_init(&kb_style);
  lv_style_set_bg_color(&kb_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
  lv_style_set_bg_opa(&kb_style, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_style_set_border_width(&kb_style, LV_STATE_DEFAULT, 0);

  lv_obj_add_style(kb, LV_OBJ_PART_MAIN, &kb_style);

  lv_keyboard_set_textarea(kb, ta_question);
  lv_obj_set_event_cb(kb, keyboard_event_cb);  // Attach event callback
}

void keyboard_event_cb(lv_obj_t* kb, lv_event_t event) {
  lv_keyboard_def_event_cb(kb, event);  // Handle default keyboard behavior

  if (event == LV_EVENT_APPLY) {
    // Retrieve the text from the text area linked to the keyboard
    lv_obj_t* ta_question = lv_keyboard_get_textarea(kb);
    const char* text = lv_textarea_get_text(ta_question);

    userInputText = String(text);  // Store user input
    Serial.println("User Input: " + userInputText);

    // Process the input with the captured image
    if (!capturedImageBase64.isEmpty()) {
      exampleVisionQuestionWithImage(userInputText, capturedImageBase64);
    } else {
      displayMessage("Error: Failed to encode image.");
    }
  }

  if (event == LV_EVENT_CANCEL) {


    // Retrieve the text area linked to the keyboard
    lv_obj_t* ta_question = lv_keyboard_get_textarea(kb);

    if (ta_question) {
      // Clear the text area content (you can set an empty string)
      lv_textarea_set_text(ta_question, "");
    }
  }
}

void captureAndProcessImage() {


  // Capture the image from the camera
  capturedFrameBuffer = esp_camera_fb_get();

  if (!capturedFrameBuffer) {
    Serial.println("Camera capture failed");
    displayMessage("Error: Camera capture failed");
  }

  // Encode the captured image to Base64
  capturedImageBase64 = encodeImageToBase64(capturedFrameBuffer->buf, capturedFrameBuffer->len);

  // Immediately turn off the flash when the button is pressed
  if (flashOn) {
    digitalWrite(FLASH_GPIO_NUM, LOW);  // Turn off flash (GPIO 4)
    flashOn = false;
    Serial.println("Flash OFF");
  }


  beep();

  if (capturedImageBase64.isEmpty()) {
    Serial.println("Failed to encode the image!");
    displayMessage("Error: Image encoding failed");

    esp_camera_fb_return(capturedFrameBuffer);  // Return the frame buffer
    capturedFrameBuffer = nullptr;
  }

  Serial.println("Image encoded to Base64");


  // Stop the live feed by setting isCaptureMode to true
  isCaptureMode = true;

  // Display the captured image
  tft.fillScreen(TFT_BLACK);  // Clear the screen
  drawBorders();              // Draw borders around the screen

  if (capturedFrameBuffer) {
    TJpgDec.drawJpg(80, 5, capturedFrameBuffer->buf, capturedFrameBuffer->len);
  }

  displayLargeCenteredMessage("Opening the keyboard...");

  delay(3000);  // Wait for 3 seconds

  // Show the keyboard for user input
  createKeyboard();

  drawBorders();
}

void exampleVisionQuestionWithImage(const String& userInputText, const String& base64Image) {
  String url = "data:image/jpeg;base64," + base64Image;

  DynamicJsonDocument doc(4096);
  doc["model"] = "gpt-4o";
  JsonArray messages = doc.createNestedArray("messages");
  JsonObject message = messages.createNestedObject();
  message["role"] = "user";
  JsonArray content = message.createNestedArray("content");

  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = userInputText;  // User question

  JsonObject imageContent = content.createNestedObject();
  imageContent["type"] = "image_url";
  JsonObject imageUrlObject = imageContent.createNestedObject("image_url");
  imageUrlObject["url"] = url;
  imageContent["image_url"]["detail"] = "auto";

  doc["max_tokens"] = 400;
  String payload;
  serializeJson(doc, payload);

  Serial.println("Sending request to ChatGPT...");
  Serial.println("User Input: " + userInputText);
  Serial.println("Base64 Image Size: " + String(base64Image.length()));

  // Clear the screen
  tft.fillScreen(TFT_BLACK);
  drawBorders();

  // Display the captured image
  if (capturedFrameBuffer) {
    TJpgDec.drawJpg(80, 5, capturedFrameBuffer->buf, capturedFrameBuffer->len);
  }

  // Display the "Sending request" message
  displayLargeCenteredMessage("Sending request to ChatGPT...");

  String result;
  if (sendPostRequest(payload, result)) {
    displayCapturedImageAndResponse(result);
    Serial.println("Response received from ChatGPT:");
    Serial.println(result);
  } else {
    Serial.println("Error: Unable to get a response from ChatGPT");
    displayCapturedImageAndResponse("Error: Unable to get response from ChatGPT");
  }
}


bool sendPostRequest(const String& payload, String& result) {
  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiKey);

  int httpCode = http.POST(payload);
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      result = http.getString();
      http.end();
      return true;
    } else {
      Serial.println("HTTP Error Code: " + String(httpCode));
    }
  } else {
    Serial.println("POST request failed.");
  }
  http.end();
  return false;
}



void displayCapturedImageAndResponse(const String& result) {
  // Parse the ChatGPT response
  StaticJsonDocument<2000> doc;
  DeserializationError error = deserializeJson(doc, result);
  const char* response = "Error: Parsing failed";  // Default message if parsing fails
  if (!error) {
    response = doc["choices"][0]["message"]["content"];
  }

  // Display the captured image
  if (capturedFrameBuffer) {
    TJpgDec.drawJpg(80, 5, capturedFrameBuffer->buf, capturedFrameBuffer->len);
  }


  // Display the response message
  displayMessage(response);  // Custom function for displaying message

  while (!isResponseDisplayed) {
    uint16_t touchX = 0, touchY = 0;
    if (tft.getTouch(&touchX, &touchY, 600)) {  // If touch detected
      Serial.println("Touch detected, reinitializing display...");
      isResponseDisplayed = 1;
    }
    delay(10);
  }

  if (isResponseDisplayed) {
    isResponseDisplayed = 0;
    reinitializeDisplay();  // Reinitialize display on touch
  }
}


void reinitializeDisplay() {
  // Clear the screen and reset the display
  tft.fillScreen(TFT_WHITE);  // Clear the screen
  drawBorders();              // Draw borders around the screen

  // Reinitialize any required settings or layouts
  capturedFrameBuffer = nullptr;
  capturedImageBase64 = "";
  isCaptureMode = false;

  Serial.println("Display reinitialized successfully");

  // Load the initial layout for the interface (if you are using a layout function)
  lv_layout();  // This should load the layout you want to display
}


void loop() {
  static unsigned long lastPressTime = 0;  // Debounce button press
  unsigned long currentMillis = millis();

  // Handle button press for toggling flash and capturing image
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Check debounce time (ignores multiple button presses in quick succession)
    if (currentMillis - lastPressTime > 200) {
      lastPressTime = currentMillis;


      // Proceed with capturing the image
      displayLargeCenteredMessage("Capturing Image...");
      captureAndProcessImage();
    }

    // Wait until the button is released to avoid multiple triggers
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(10);  // Debounce and wait for release
    }
  }

  // Show live camera feed if not in capture mode and not displaying the response
  if (!isCaptureMode && !isResponseDisplayed) {
    displayCameraFeed();
  }

  // Check for touch event only after the image and response have been displayed
  if (isResponseDisplayed) {
    uint16_t touchX = 0, touchY = 0;
    if (tft.getTouch(&touchX, &touchY, 600)) {  // If touch detected
      Serial.println("Touch detected, reinitializing display...");
      reinitializeDisplay();        // Reinitialize display on touch
      isResponseDisplayed = false;  // Reset the flag after reinitialization
    }
  }

  lv_task_handler();  // Handle LVGL tasks
  delay(5);           // Small delay to balance responsiveness and CPU usage
}


// Function to handle touchpad input for LVGL
bool my_touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data) {
  uint16_t touchX, touchY;

  bool touched = tft.getTouch(&touchX, &touchY, 600);  // Get touch input from the screen, with a threshold of 600 (sensitivity)

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;  // Set the state as "pressed" (touch detected)
    data->point.x = touchX;
    data->point.y = touchY;
  }

  return false;
}


void drawBorders() {
  tft.drawRect(78, 2, 324, 246, TFT_WHITE);
  tft.drawRect(3, tft.height() - 70, 472, 68, TFT_WHITE);
}

void displayMessage(const String& message) {
  tft.fillRect(5, tft.height() - 65, 464, 60, TFT_BLACK);  // Reserve bottom section for messages with black background
  tft.setTextColor(TFT_WHITE, TFT_BLACK);                  // White text on black background
  tft.setTextSize(1);
  tft.setCursor(5, tft.height() - 65);

  int cursorX = 5, cursorY = tft.height() - 65;
  String currentWord;

  for (char c : message) {
    if (c == ' ' || c == '\n') {
      if (cursorX + tft.textWidth(currentWord) > tft.width() - 5) {
        cursorX = 5;
        cursorY += 10;
      }
      if (cursorY >= tft.height() - 5) break;
      tft.setCursor(cursorX, cursorY);
      tft.print(currentWord);
      cursorX += tft.textWidth(currentWord) + tft.textWidth(" ");
      currentWord = "";
    } else {
      currentWord += c;
    }
  }

  if (!currentWord.isEmpty()) {
    if (cursorX + tft.textWidth(currentWord) > tft.width() - 5) {
      cursorX = 5;
      cursorY += 10;
    }
    tft.setCursor(cursorX, cursorY);
    tft.print(currentWord);
  }
}

void displayLargeCenteredMessage(const String& message) {
  tft.fillRect(0, tft.height() - 70, tft.width(), 70, TFT_BLACK);  // Clear the message area with black background
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // White text on black background

  int x = (tft.width() - tft.textWidth(message)) / 2;
  int y = tft.height() - 50;  // Ensure this doesn't overlap with buttons

  tft.setCursor(x, y);
  tft.print(message);
  drawBorders();
}

void beep() {
  digitalWrite(3, HIGH);
  delay(300);
  digitalWrite(3, LOW);
}
