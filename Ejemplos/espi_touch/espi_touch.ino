// Incluye las librerías necesarias.
// Asegúrate de que TFT_eSPI esté configurada en tu User_Setup.h
#include <SPI.h>
#include <TFT_eSPI.h>

// Crea una única instancia del objeto TFT_eSPI
TFT_eSPI tft = TFT_eSPI();

void setup(void) {
  // Inicializa el puerto serie para la depuración
  Serial.begin(115200);

  // Inicializa la pantalla
  tft.init();

  // Opcional: Establece una rotación de la pantalla (0 a 3)
  tft.setRotation(1);

  // Llena la pantalla con un color de fondo
  tft.fillScreen(TFT_BLACK);
  
  // Muestra un mensaje en la pantalla
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 10);
  tft.println("Toca la pantalla");
  tft.println("para dibujar.");
}

void loop() {
  uint16_t x, y; // Variables para almacenar las coordenadas del toque

  // getTouch() retorna 'true' si se detecta un toque.
  // Las coordenadas se guardan en las variables x e y que se pasan por referencia.
  if (tft.getTouch(&x, &y)) {
    // Si se ha tocado la pantalla, dibuja un círculo en esas coordenadas
    tft.fillCircle(x, y, 5, TFT_RED);
    
    // Opcional: Muestra las coordenadas en el monitor serie para depuración
    Serial.print("Coordenada X: ");
    Serial.print(x);
    Serial.print(", Coordenada Y: ");
    Serial.println(y);
  }
}