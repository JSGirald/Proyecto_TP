/*******************************************************************
    Basado en el ejemplo:
    TFT_eSPI button example for the ESP32 Cheap Yellow Display.

 *******************************************************************/

// ----------------------------
// Standard Libraries
// ----------------------------

#include <SPI.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <XPT2046_Bitbang.h>
// A library for interfacing with the touch screen

#include <TFT_eSPI.h>

// A library for interfacing with LCD displays

// ----------------------------
// Touch Screen pins
// ----------------------------

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// ----------------------------

XPT2046_Bitbang ts(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS, 320, 240);

TFT_eSPI tft = TFT_eSPI();

TFT_eSPI_Button key[4];

char* nombres[4] = {"Luz 1","Luz 2","Son 1","Son 2"};

void setup() {
  Serial.begin(115200);
  
  // Start the SPI for the touch screen and init the TS library
  ts.begin();
  //ts.setRotation(2);
  
  // Start the tft display and set it to black
  tft.init();
  tft.setRotation(1); // 0, 2 retrato, 1, 3 apaisado

  // Limpia la pantalla y seleccione la fuente
  tft.fillScreen(TFT_BLACK);
  //tft.setFreeFont(&FreeBold18pt7b);
  tft.setFreeFont(&FreeMonoBold18pt7b);
  drawButtons();

  // Calibracion:
  // Valores de calibracion para Portrait (vertical)
  //uint16_t xMin_P = 3700, xMax_P = 350;
  //uint16_t yMin_P = 292, yMax_P = 3800;

  //ts.setCalibration(xMin_P, xMax_P, yMin_P, yMax_P);

}

void drawButtons() {
  uint16_t bWidth = TFT_HEIGHT/2;
  uint16_t bHeight = TFT_WIDTH/2;
  //uint16_t bWidth = TFT_WIDTH/2;
  //uint16_t bHeight = TFT_HEIGHT/2;

  Serial.println(TFT_WIDTH);
  Serial.println(TFT_HEIGHT);

  // Generate buttons with different size X deltas
  for (int i = 0; i < 4; i++) {
    Serial.println(bWidth * (i%2) + bWidth/2);
    
    key[i].initButton(&tft,
                      bWidth * (i%2) + bWidth/2,
                      bHeight * (i/2) + bHeight/2,
                      bWidth,
                      bHeight,
                      TFT_BLACK, // Outline
                      TFT_BLUE, // Fill
                      TFT_YELLOW, // Text
                      nombres[i],
                      1);

    key[i].drawButton(false);
  }
}

void loop() {
  TouchPoint p = ts.getTouch();
  // Adjust press state of each key appropriately
  for (uint8_t b = 0; b < 4; b++) {
    if ((p.zRaw > 0) && key[b].contains(p.x, p.y)) {
      key[b].press(true);  // tell the button it is pressed

      Serial.println("CoordR: " + (String)p.xRaw + " " + (String)p.yRaw);
      Serial.println("Coord: " + (String)p.x + " " + (String)p.y);
    } else {
      key[b].press(false);  // tell the button it is NOT pressed
    }
  }

  // Check if any key has changed state
  for (uint8_t b = 0; b < 4; b++) {
    // If button was just pressed, redraw inverted button
    if (key[b].justPressed()) {
      Serial.printf("Button %d pressed\n", b);
      key[b].drawButton(true, nombres[b]);
    }

    // If button was just released, redraw normal color button
    if (key[b].justReleased()) {
      Serial.printf("Button %d released\n", b);
      // Serial.println("Button " + (String)b + " released");
      key[b].drawButton(false, nombres[b]);
    }
  }
  delay(50);
}