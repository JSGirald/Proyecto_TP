// Librerías de wifi
#include <WiFi.h>
#include <WebServer.h>

// AudioTools
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Disk/AudioSourceSD.h" // o AudioSourceIdxSD.h
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

// La cola debe usar un array char en lugar de un objeto String
#define MAX_FILENAME_LEN 64 // macro con la longitud máxima del nombre de archivo

// Constantes
const char* ap_ssid = "Control_1";
const char* ap_pass = "123456789";
const char* html PROGMEM = "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"UTF-8\"><title>Principal</title><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>.enlaceboton{display:block;font-family:verdana,arial,sans-serif;font-size:10pt;font-weight:700;padding:8px;margin:8px;background-color:#ffc;color:#333;text-decoration:none;border:2px solid #666;text-align:center}.enlaceboton:hover{background-color:#7a5;color:#fff}</style></head><body><h1>Efectos:</h1>";
const char* html_fin PROGMEM = "</body></html>";
const int chipSelect = PIN_AUDIO_KIT_SD_CARD_CS;
const char* raiz = "/"; // directorio raiz
const char* ext = "mp3"; // 

// ... y demás variables globales
WebServer servidor(80);
AudioSourceSD fuente(raiz, ext, chipSelect);
AudioBoardStream kit(AudioKitEs8388V1);
MP3DecoderHelix decode;
AudioPlayer player(fuente, kit, decode); // ****** objeto player ******
String efectos = ""; // Cadena en la que se van a leer los nombres de archivo

QueueHandle_t colaAudio;  // Manejador de la cola de audios

// Variables que almacenan el html base
char botones[1024];  // concatenación de TODOS los botones (enlaces)
// html de los botones indivisuales que representan cada efecto (enlace para realizar el GET)
char boton[256] = "<a class=\"enlaceboton\" href=\"\">%s</a>";  // se insertarán los nombres

void manejadorHTML() {
  String nombre_str; // usar variable auxiliar tipo String dentro de la función

  nombre_str = servidor.arg("NOMBRE");
  Serial.print("ManejadorHTML: " );
  Serial.println(nombre_str);
  
  if (nombre_str != "") {
    char nombre[MAX_FILENAME_LEN]; // crea una variable tipo array char para guardar el nombre 
    strncpy(nombre, nombre_str.c_str(), sizeof(nombre)); // copia nombre
    nombre[sizeof(nombre) - 1] = '\0'; // la cadena debe terminar en caracter nulo

    if (xQueueSend(colaAudio, &nombre, portMAX_DELAY) == pdPASS) {
      Serial.println("Enviado cola: " + nombre_str);
      // player.playPath(nombre.c_str());

      // servidor.send(200, "text/html", String(html) + String(botones) + String(html_fin));
    } else {
      servidor.send(500, "text/html", "Error al enviar el nombre del archivo de sonido.");
      return;
    }
  }
  servidor.send(200, "text/html", String(html) + String(botones) + String(html_fin));
}

void enviaEfectos() {
  if (efectos != "") {
    Serial.println("Recibido endpoint efectos");
    Serial.println(efectos);
    servidor.send(200, "text/plain", efectos);
  }
}

// Tarea 1: Servidor Web
// Se ejecutará en el Core 0 (el core de comunicación)
void ServidorWebTask(void* parameter) {
  servidor.on("/", manejadorHTML);

  servidor.on("/efectos", enviaEfectos); // ENDPOINT para leer los nombres de archivo

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
  // Variable para guardar el nombre del archivo a reproducir ???
  char archivo[MAX_FILENAME_LEN];
  // Código de setup de audio aquí 
  auto config = kit.defaultConfig(TX_MODE);
  config.sd_active = true;
  kit.begin(config);

  fuente.begin();
  long cuenta = fuente.size();
  String lista[64];
  char buffer[64];

  for (int i = 0; i < cuenta; i++) {
    fuente.selectStream(i);
    Serial.print(i);
    Serial.println(fuente.toStr());

    sprintf(buffer, fuente.toStr());
    lista[cuenta] = buffer;
    sprintf(boton, "<a class=\"enlaceboton\" href=\"http://192.168.4.1\?NOMBRE=%s\">%s</a>", fuente.toStr(), fuente.toStr());
    // Serial.println(boton);
    sprintf(botones + strlen(botones), boton);
    efectos += "\\\"" + String(fuente.toStr()) + "\\\"";
    if ((i + 1) < cuenta) {
      efectos += ",";
    } else {
      efectos = "\"[" + efectos + "]\"";
    }
  }
  Serial.println(efectos);
  // configuración de los botones de la placa
  //kit.addDefaultActions();
  //kit.addAction(kit.getKey(1), startStop);
  //kit.addAction(kit.getKey(4), next);
  //kit.addAction(kit.getKey(3), previous);

  // setup player
  player.setVolume(0.75);
  player.begin();
  player.setAutoNext(false);


  // player.playPath("/terror.mp3");
  // player.playPath("/terror-scream.mp3");

  // Bucle infinito de la tarea
  for (;;) {
    //player.copy();
    //kit.processActions();
    if (xQueueReceive(colaAudio, &archivo, portMAX_DELAY) == pdPASS) {
      //if (!player.isActive()) {
        Serial.print("Recibido: ");
        Serial.println(archivo);
      //}
        //player.playPath(archivo.c_str());
        player.playPath(archivo);
        //Serial.println(player.copy());
    }
  }
}

// Configuración principal del programa
void setup() {
  Serial.begin(115200);

  // Conectar a WiFi (puedes hacerlo en STA o SoftAP)
  WiFi.softAP(ap_ssid, ap_pass);
  Serial.print("IP del AP: ");
  Serial.println(WiFi.softAPIP());

  // Crear la cola
  // colaAudio = xQueueCreate(1, sizeof(String));
  colaAudio = xQueueCreate(1, sizeof(char[MAX_FILENAME_LEN]));

  if (colaAudio == NULL) {
    Serial.println("Error, no se ha podido crear la cola de audio ...");
  }

  //fuente.begin();

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
    5,
    NULL,
    1);  // **Core en el que se ejecuta (1)**
}

// El loop() principal puede quedar vacío
void loop() {
  // Las tareas se ejecutan de forma independiente.
  // No necesitas poner nada aquí.
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

