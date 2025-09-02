#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

// Constantes
const char* ap_ssid = "Control_1";
const char* ap_pass = "123456789";
const int chipSelect = PIN_AUDIO_KIT_SD_CARD_CS;
const char* html PROGMEM = "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"UTF-8\"><title>Principal</title><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>.enlaceboton{display:block;font-family:verdana,arial,sans-serif;font-size:10pt;font-weight:700;padding:8px;margin:8px;background-color:#ffc;color:#333;text-decoration:none;border:2px solid #666;text-align:center}.enlaceboton:hover{background-color:#7a5;color:#fff}</style></head><body><h1>Efectos:</h1>";
const char* html_fin PROGMEM = "</body></html>";

// ... y demás variables globales
WebServer servidor(80);
AudioBoardStream i2s(AudioKitEs8388V1);
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix());  // Decoding stream
StreamCopy copier;
File audioFile;
QueueHandle_t colaAudio;  // Manejador de la cola de audios

char botones[1024];  // concatenación de TODOS los botones (enlaces)
// html de los botones indivisuales que representan cada efecto (enlace para realizar el GET)
char boton[256] = "<a class=\"enlaceboton\" href=\"\">%s</a>";  // se insertarán los nombres

void manejadorHTML() {
  String nombre;
  nombre = servidor.arg("NOMBRE");
  Serial.print("Argumento: ");
  Serial.println(nombre);
  if (nombre != "") {
    if (xQueueSend(colaAudio, &nombre, portMAX_DELAY) == pdPASS) {
      servidor.send(200, "text/html", String(html) + String(botones) + String(html_fin));
      Serial.println("Enviado: " + nombre);
    } else {
      servidor.send(500, "text/html", "Error al enviar el nombre del archivo de sonido.");
    }
  }
  servidor.send(200, "text/html", String(html) + String(botones) + String(html_fin));
}

// Tarea 1: Servidor Web
// Se ejecutará en el Core 0 (el core de comunicación)
void ServidorWebTask(void* parameter) {
  servidor.on("/", manejadorHTML);
  // servidor.on("/", []() {
  //   servidor.send(200, "text/html", String(html) + String(botones) + String(html_fin));
  // });
  servidor.begin();

  // Bucle infinito de la tarea
  for (;;) {
    servidor.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Pequeño delay para ceder el procesador
  }
}

// Tarea 2: Reproducción de Audio
// Se ejecutará en el Core 1 (el core de las operaciones en segundo plano)
void AudioTask(void* parameter) {
  // Variable para guardar el nombre del archivo a reproducir
  String archivo;

  // Tu código de setup de audio aquí (SD, i2s, decoder)
  auto config = i2s.defaultConfig(TX_MODE);
  config.sd_active = true;
  i2s.begin(config);

  // inicia la tarjeta SD
  SD.begin(chipSelect);
  audioFile = SD.open("/terror-scream.mp3");  // Seguramente esto ya no sea necesario aquí

  // Lee directorio raiz
  Serial.println("Encontrados " + String(lsRaiz()) + " elementos.");

  // setup I2S based on sampling rate provided by decoder
  decoder.begin();

  // begin copy
  copier.begin(decoder, audioFile);

  //   if (!copier.copy()) {
  //   // Si la copia terminó, detener la reproducción
  //     stop();
  //   }

  // Bucle infinito de la tarea
  for (;;) {

    if (xQueueReceive(colaAudio, &archivo, portMAX_DELAY) == pdPASS) {
      // archivo = "/" + archivo;
      // Serial.print("Nombre de archivo recibido: ");
      // Serial.println(archivo);
      // File audio = SD.open(archivo.c_str());

      // if (audio) {
      //   i2s.begin();
      //   // copier.begin(audio, i2s);
      //   copier.begin(decoder, audio);
      //   if (copier.copy()) {
      //     Serial.print("|>");
      //   } else {
      //     copier.end();
      //     audio.close();
      //     Serial.println("\nReproducción terminada ...");
      //   }
      // } else {
      //   Serial.println("Error, archivo no encontrado ...");
      // }
    }

    // if (!copier.copy()) {
    // // Si la copia terminó, detener la reproducción
    //   stop();
    // }
    if (!copier.copy()) {
    // Si la copia terminó, detener la reproducción
      stop();
    }



  }
  // No se necesita vTaskDelay aquí ya que copier.copy() cede el procesador
}

uint8_t lsRaiz() {
  Serial.println("Archivos del directorio raíz ...");
  char buffer[64];   // espacio para guardar el texto
  String lista[64];  // lista de nombres de archivos
  uint8_t contador = 0;

  // Abrir directorio
  File raiz = SD.open("/");
  if (!raiz) {
    Serial.println("No ha sido posible abrir el directorio raíz ...");
    return contador;
  }

  // Iterar
  File entrada = raiz.openNextFile();
  while (entrada) {
    if (!entrada.isDirectory()) {
      sprintf(buffer, entrada.name());
      lista[contador] = buffer;
      sprintf(boton, "<a class=\"enlaceboton\" href=\"http://192.168.4.1\?NOMBRE=%s\">%s</a>", entrada.name(), entrada.name());
      Serial.println(boton);
      sprintf(botones + strlen(botones), boton);
      contador++;
    }
    entrada = raiz.openNextFile();
  }
  Serial.println("------");
  Serial.println(botones);
  return contador;
}

// Configuración principal del programa
void setup() {
  Serial.begin(115200);

  // Conectar a WiFi (puedes hacerlo en STA o SoftAP)
  WiFi.softAP(ap_ssid, ap_pass);
  Serial.print("IP del AP: ");
  Serial.println(WiFi.softAPIP());

  // Crear la cola
  colaAudio = xQueueCreate(5, sizeof(String));
  if (colaAudio == NULL) {
    Serial.println("Error, no se ha podido crear la cola de audio ...");
  }

  // Crear y lanzar la tarea del Servidor Web en el Core 0
  xTaskCreatePinnedToCore(
    ServidorWebTask,  // Nombre de la función de la tarea
    "Servidor Web",   // Nombre descriptivo
    10000,            // Tamaño de la pila (bytes)
    NULL,             // Parámetros de la tarea
    1,                // Prioridad de la tarea (1 es baja, 0 es la más baja)
    NULL,             // Handle de la tarea (opcional)
    0);               // **Core en el que se ejecuta (0)**

  // Crear y lanzar la tarea de Audio en el Core 1
  xTaskCreatePinnedToCore(
    AudioTask,
    "Audio",
    10000,
    NULL,
    1,
    NULL,
    1);  // **Core en el que se ejecuta (1)**
}

// El loop() principal puede quedar vacío
void loop() {
  // Las tareas se ejecutan de forma independiente.
  // No necesitas poner nada aquí.
}