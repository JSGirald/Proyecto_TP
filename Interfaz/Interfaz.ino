/*******************************************************************
 * * INTERFAZ DE CONTROL PARA DISPLAY ESP32 (VERSIÓN MINIMALISTA)
 * *
 * * OBJETIVO: Mantener solo la conexión WiFi, el menú principal (MENU)
 * * y la pantalla de control de luces (LUCES).
 * *
 * * CAMBIO: Menú principal ahora tiene botones LUCES y SONIDO, donde
 * * SONIDO solo da feedback visual y no cambia de pantalla.
 * *******************************************************************/

// ----------------------------
// Librerías Estándar
// ----------------------------
#include <SPI.h>
#include <WiFi.h> // Librería necesaria para la conectividad WiFi
#include <HTTPClient.h>

// ----------------------------
// Librerías Adicionales
// ----------------------------
#include <XPT2046_Bitbang.h> // Librería para la pantalla táctil
#include <TFT_eSPI.h>        // Librería para la pantalla LCD

// ----------------------------
// DEFINICIONES DE PINES DE HARDWARE (AJUSTAR SEGÚN EL CABLEADO)
// ----------------------------
// Pines del Touch XPT2046
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// ----------------------------
// CONFIGURACIÓN WIFI (Modo Estación - STA)
// ----------------------------
const char* WIFI_STA_SSID = "Control_1";
const char* WIFI_STA_PASS = "123456789";
IPAddress IP_ESTATICA(192, 168, 4, 1); // Usada solo como referencia

// ----------------------------
// Inicialización de Librerías y Objetos
// ----------------------------
XPT2046_Bitbang ts(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS, 320, 240);
TFT_eSPI tft = TFT_eSPI();
TFT_eSPI_Button key[8]; // Array de botones con capacidad para 8

// ----------------------------
// VARIABLES DE ESTADO GLOBAL Y ENUMERACIÓN DE PANTALLAS
// ----------------------------
enum TipoPantalla {
    PANTALLA_WIFI_STATUS = 0, // Muestra estado y botón CONECTAR (si desconectado)
    PANTALLA_MENU = 1,        // Menú principal (si conectado)
    PANTALLA_LUCES = 2,       // Control de Luz 1 y Luz 2
    PANTALLA_SONIDO = 3       // Control de sonido
    // PANTALLA_SONIDO (ELIMINADA)
};

TipoPantalla pantalla_actual = PANTALLA_WIFI_STATUS;
uint8_t NUM_BOTONES_ACTIVOS = 0; // Rastrea cuántos botones de 'key[]' están activos en la pantalla actual

// Nombres y estados de los botones de control (solo necesitamos Luz 1 y Luz 2)
char nombres[2][20] = {"Luz 1", "Luz 2"}; 
bool estado_botones[2] = {false, false}; // Estado interno ON/OFF de Luz 1 y Luz 2
bool inicializada = false; // Flag para saber si la pantalla debe ser dibujada (solo una vez)

// ----------------------------
// Prototipos de Funciones (Nombres en español)
// ----------------------------
void establecer_conexion_inicial(); 
void actualizar_texto_luz(uint8_t indice, bool estado);

// Funciones unificadas de control
void controlar_wifi_status();
void controlar_menu();
void controlar_luces();
void controlar_sonido();

// ----------------------------
// SETUP
// ----------------------------
void setup() {
  Serial.begin(115200);
  
  // --- Configuración Inicial TFT y Touch ---
  tft.init();
  tft.setRotation(1); // Orientación Apaisada (Landscape: 320x240)
  ts.begin();
  
  // Calibración (Ajusta estos valores según tus pruebas!)
  uint16_t xMin = 300, xMax = 3800;
  uint16_t yMin = 300, yMax = 3800;
  ts.setCalibration(xMin, xMax, yMin, yMax);
  
  // --- Control de Flujo Inicial ---
  establecer_conexion_inicial(); 

  // --- Leer la lista de archivos de sonido disponibles ---
  enviar_peticion_efecto(0);
  
  // Configura el font para la interfaz
  tft.setFreeFont(&FreeMonoBold18pt7b);
}

// ----------------------------
// FUNCIÓN DE CONEXIÓN WIFI (Modo Estación - STA)
// ----------------------------
void establecer_conexion_inicial() {
    // Inicializa la pantalla para mostrar el estado de conexión
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM); // Centro Medio
    tft.setFreeFont(&FreeMonoBold18pt7b);

    tft.drawString("Conectando a WiFi...", tft.width() / 2, tft.height() / 2 - 30, 2);
    tft.drawString(WIFI_STA_SSID, tft.width() / 2, tft.height() / 2, 4);

    Serial.println("\n--- Conectando en modo Estación (STA) ---");

    // Conexión
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {
        delay(500);
        Serial.print(".");
        tft.fillRect(0, tft.height() / 2 + 30, tft.width(), 30, TFT_BLACK);
        tft.drawString("Intentos: " + String(intentos + 1), tft.width() / 2, tft.height() / 2 + 30, 2);
        intentos++;
    }
    
    // Establece la pantalla a mostrar después del intento
    tft.fillScreen(TFT_BLACK); // Limpia
    if (WiFi.status() == WL_CONNECTED) {
        tft.setTextColor(TFT_GREEN);
        tft.drawString("CONEXIÓN EXITOSA", tft.width() / 2, tft.height() / 2 - 30, 2);
        tft.drawString("IP: " + WiFi.localIP().toString(), tft.width() / 2, tft.height() / 2 + 10, 2);
        pantalla_actual = PANTALLA_MENU;
    } else {
        tft.setTextColor(TFT_RED);
        tft.drawString("FALLO DE CONEXIÓN", tft.width() / 2, tft.height() / 2 - 30, 2);
        tft.drawString("Presione CONECTAR", tft.width() / 2, tft.height() / 2 + 10, 2);
        pantalla_actual = PANTALLA_WIFI_STATUS;
    }
    delay(1500); // Muestra el resultado brevemente
    inicializada = false; // Forzar el primer dibujo de la pantalla inicial
}

// ----------------------------
// FUNCIONES DE CONTROL (Dibujo + Lógica de pulsación)
// ----------------------------

// Actualiza el texto de los botones de luz para reflejar su estado ON/OFF.
void actualizar_texto_luz(uint8_t indice, bool estado) {
    // Los índices del botón son 1 (Luz 1) y 2 (Luz 2)
    if (indice < 2) { 
        sprintf(nombres[indice], "Luz %d (%s)", indice + 1, estado ? "ON" : "OFF"); 
        key[indice + 1].drawButton(false, nombres[indice]); 
    }
}

// PANTALLA 0: Estado WiFi (Controla la conexión)
void controlar_wifi_status() {
    if (!inicializada) {
        tft.fillScreen(TFT_BLACK);
        tft.setFreeFont(&FreeMonoBold18pt7b);
        tft.setTextDatum(MC_DATUM);

        if (WiFi.status() != WL_CONNECTED) {
            tft.setTextColor(TFT_RED);
            tft.drawString("WiFi Desconectado", tft.width() / 2, tft.height() / 4, 1);
            
            // Botón GRANDE para Conectar (key[0])
            NUM_BOTONES_ACTIVOS = 1;
            key[0].initButton(&tft,
                              tft.width() / 2,        // Centro X
                              tft.height() * 0.7,     // Centro Y
                              tft.width() * 0.8,      // Ancho (80% del total)
                              tft.height() * 0.4,     // Alto (40% del total)
                              TFT_RED,
                              TFT_DARKGREY,
                              TFT_WHITE,
                              "CONECTAR",
                              1);
            key[0].drawButton(false);
        }
        inicializada = true;
    }

    // --- Lógica de pulsación ---
    TouchPoint p = ts.getTouch();
    bool touched = (p.zRaw > 0);

    for (uint8_t b = 0; b < NUM_BOTONES_ACTIVOS; b++) {
        bool contained = key[b].contains(p.x, p.y);
        key[b].press(contained && touched);

        if (key[b].justPressed()) {
            key[b].drawButton(true, "CONECTAR");
        }
        if (key[b].justReleased()) {
            key[b].drawButton(false, "CONECTAR");
            if (b == 0) { // Botón CONECTAR
                Serial.println("Reintentando conexión...");
                inicializada = false; // Forzar redibujo
                establecer_conexion_inicial(); // Vuelve a intentar la conexión
            }
        }
    }
}

// PANTALLA 1: Menú Principal (Botones LUCES y SONIDO - SONIDO inactivo)
void controlar_menu() {
    if (!inicializada) {
        tft.fillScreen(TFT_BLACK);
        // TÍTULO DE PANTALLA 
        tft.setTextColor(TFT_YELLOW);
        tft.setFreeFont(&FreeSansBold9pt7b); 
        tft.drawString("MENU PRINCIPAL", tft.width() / 2, 20);

        tft.setFreeFont(&FreeMonoBold18pt7b);
        tft.setTextDatum(MC_DATUM);

        // Botones de navegación (2 columnas, 1 fila)
        uint16_t ancho_boton = tft.width() / 2;
        uint16_t alto_boton = tft.height() / 2;
        
        NUM_BOTONES_ACTIVOS = 2; // LUCES (key[0]) y SONIDO (key[1])

        // Botón 0: LUCES (Mitad izquierda)
        key[0].initButton(&tft,
                          ancho_boton / 2,          // Centro X
                          60 + alto_boton / 2,           // Centro Y
                          ancho_boton,              // Ancho
                          alto_boton,               // Alto
                          TFT_LIGHTGREY,
                          TFT_BLUE,
                          TFT_YELLOW,
                          "LUCES",
                          1);
        key[0].drawButton(false);

        // Botón 1: SONIDO (Mitad derecha - Inactivo)
        key[1].initButton(&tft,
                          ancho_boton + ancho_boton / 2, // Centro X
                          60 + alto_boton / 2,                // Centro Y
                          ancho_boton,
                          alto_boton,
                          TFT_LIGHTGREY,
                          TFT_PURPLE,
                          TFT_YELLOW,
                          "SONIDO",
                          1);
        key[1].drawButton(false);
        inicializada = true;
    }
    
    TipoPantalla pantalla_destino = PANTALLA_MENU;
    
    // --- Lógica de pulsación ---
    TouchPoint p = ts.getTouch();
    bool touched = (p.zRaw > 0);

    for (uint8_t b = 0; b < NUM_BOTONES_ACTIVOS; b++) {
        // La etiqueta para redibujar
        const char* label = (b == 0) ? "LUCES" : "SONIDO";
        bool contained = key[b].contains(p.x, p.y);
        
        key[b].press(contained && touched); 

        if (key[b].justPressed()) {
            key[b].drawButton(true, label);
        }
        
        if (key[b].justReleased()) {
            key[b].drawButton(false, label); 
            
            if (b == 0) { // Botón LUCES
                Serial.println("TRANSICION (SOLICITADA): MENU -> LUCES");
                pantalla_destino = PANTALLA_LUCES;
            } else if (b == 1) { // Botón SONIDO - No hace nada, solo feedback visual
                Serial.println("Botón SONIDO pulsado, sin acción.");
                // pantalla_destino se mantiene en PANTALLA_MENU
                pantalla_destino = PANTALLA_SONIDO;
            }
        }
    }
    
    // Aplica la transición de estado
    if (pantalla_destino != PANTALLA_MENU) {
        pantalla_actual = pantalla_destino;
        inicializada = false;
    }
}


// PANTALLA 2: Control de Luces (ATRAS + Luz 1 + Luz 2)
void controlar_luces() {
    // Definimos una altura para la barra de título/atras
    const uint16_t ALTO_BARRA_ATRAS = 40; 
    const uint16_t ALTO_TITULO = 30;
    if (!inicializada) {
        tft.fillScreen(TFT_BLACK);
        tft.setFreeFont(&FreeMonoBold12pt7b);
        tft.setTextDatum(MC_DATUM); 
        
        // Botón 0: ATRAZ al MENU (Ocupa la barra superior)
        NUM_BOTONES_ACTIVOS = 3; // ATRAS (0), Luz 1 (1), Luz 2 (2)
        key[0].initButton(&tft,
                          tft.width() / 2, 
                          ALTO_BARRA_ATRAS / 2, 
                          tft.width(), 
                          ALTO_BARRA_ATRAS, 
                          TFT_DARKGREY, 
                          TFT_WHITE, 
                          TFT_BLACK, 
                          "< MENU", 
                          1);
        key[0].drawButton(false);
        
        // TÍTULO DE PANTALLA 
        tft.setTextColor(TFT_YELLOW);
        tft.setFreeFont(&FreeSansBold9pt7b); 
        tft.drawString("CONTROL DE ILUMINACION", tft.width() / 2, ALTO_BARRA_ATRAS + 15);
        tft.setFreeFont(&FreeMonoBold9pt7b);
        
        // Dibuja el grid 2x1 para los botones de control (Luz 1 y Luz 2)
        uint16_t alto_cuerpo = tft.height() - ALTO_BARRA_ATRAS - ALTO_TITULO;
        uint16_t ancho_boton = tft.width() / 2;
        
        // Bucle para los botones de Luz 1 (i=1) y Luz 2 (i=2)
        for (int i = 1; i < 3; i++) { 
            // Posiciones relativas al área debajo de la barra de título
            uint16_t centro_x = ancho_boton * (i-1) + ancho_boton / 2;
            uint16_t centro_y = alto_cuerpo / 2 + ALTO_BARRA_ATRAS + ALTO_TITULO;
            
            // Inicializa botones 1 y 2
            key[i].initButton(&tft,
                              centro_x,
                              centro_y,
                              ancho_boton,
                              alto_cuerpo,
                              TFT_DARKGREY,
                              TFT_BLUE,
                              TFT_YELLOW,
                              nombres[i-1], 
                              1); 
            
            // Actualiza el texto para reflejar el estado inicial
            actualizar_texto_luz(i-1, estado_botones[i-1]); 
        }
        inicializada = true;
    }
    
    // --- Lógica de pulsación ---
    TouchPoint p = ts.getTouch();
    bool touched = (p.zRaw > 0);
    
    for (uint8_t b = 0; b < NUM_BOTONES_ACTIVOS; b++) { // Loop 0, 1, 2
        // La etiqueta para los botones de control (b=1,2) se toma de nombres[b-1]
        const char* label = (b == 0) ? "< MENU" : nombres[b-1];
        bool contained = key[b].contains(p.x, p.y);
        
        key[b].press(contained && touched);

        // Redibujar presionado
        if (key[b].justPressed()) {
            key[b].drawButton(true, label);
        }
        
        // Lógica al soltar
        if (key[b].justReleased()) {
            if (b == 0) { // Botón [0] - Siempre ATRAS
                pantalla_actual = PANTALLA_MENU;
                inicializada = false;
                return;
            } else if (b == 1 || b == 2) { // Botones [1] y [2] - Luces 
                uint8_t estado_idx = b - 1;
                estado_botones[estado_idx] = !estado_botones[estado_idx]; // Toggle state
                // El redibujo se hace dentro de actualizar_texto_luz
                actualizar_texto_luz(estado_idx, estado_botones[estado_idx]);
                Serial.printf("Luz %d - Estado Lógico: %s\n", estado_idx + 1, estado_botones[estado_idx] ? "ON" : "OFF");
            }
        }
    }
}

// PANTALLA 3: Control de Sonido (Nueva)
void controlar_sonido() {
    const uint16_t ALTO_BARRA_ATRAS = 40;
//
//

    if (!inicializada) {
        tft.fillScreen(TFT_BLACK);
        tft.setFreeFont(&FreeMonoBold12pt7b);
        tft.setTextDatum(MC_DATUM); 

        // Botón 0: ATRAZ al MENU (Barra Superior)
        // Solo el botón de ATRAS está activo.
        NUM_BOTONES_ACTIVOS = 1; 
        
        key[0].initButton(&tft,
                          tft.width() / 2, 
                          ALTO_BARRA_ATRAS / 2, 
                          tft.width(), 
                          ALTO_BARRA_ATRAS, 
                          TFT_DARKGREY, 
                          TFT_WHITE, 
                          TFT_BLACK, 
                          (char*)"< MENU", 
                          1);
        key[0].drawButton(false);
        
        // --- TEXTO MARCADOR DE POSICIÓN ---
        tft.setTextColor(TFT_YELLOW);
        tft.setFreeFont(&FreeSansBold9pt7b); 
        tft.drawString("CONTROL DE SONIDO - VACÍO", tft.width() / 2, ALTO_BARRA_ATRAS + 50);
        tft.drawString("Aquí irá la lista de pistas y controles.", tft.width() / 2, ALTO_BARRA_ATRAS + 80);
        tft.setFreeFont(&FreeMonoBold18pt7b);
        
        inicializada = true;
    }
    
    TouchPoint p = ts.getTouch();
    bool touched = (p.zRaw > 0);
    
    // Solo se chequea el botón ATRAS (key[0])
    for (uint8_t b = 0; b < NUM_BOTONES_ACTIVOS; b++) { 
        const char* label = (b == 0) ? "< MENU (SONIDO)" : "";
        bool contained = key[b].contains(p.x, p.y);
        
        key[b].press(contained && touched);

        if (key[b].justPressed()) {
            key[b].drawButton(true, label);
        }
        
        if (key[b].justReleased()) {
            if (b == 0) { // Botón [0] - ATRAS
                pantalla_actual = PANTALLA_MENU;
                inicializada = false;
                return;
            }
        }
    }
}

// ----------------------------
// LOOP (Bucle Principal)
// ----------------------------
void loop() {
    // 1. Seleccionar qué función de control ejecutar según el estado actual
    switch(pantalla_actual) {
        case PANTALLA_WIFI_STATUS:
            controlar_wifi_status();
            break;
        case PANTALLA_MENU:
            controlar_menu();
            break;
        case PANTALLA_LUCES:
            controlar_luces();
            break;
        case PANTALLA_SONIDO:
            controlar_sonido();
            break;
        default:
            // Por defecto, intentar volver al menú si el estado es desconocido
            pantalla_actual = PANTALLA_MENU;
            inicializada = false;
            break;
    }
    
    // Pequeña pausa para evitar rebotes y mantener estabilidad
    delay(50); 
}


void enviar_peticion_efecto(uint8_t trackIndex) {
    // 1. Verificar el estado de la conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Error: No hay conexión WiFi para enviar la petición.");
        return;
    }
    
    // 2. Definir la URL completa del endpoint
    // Endpoint base: http://192.168.4.1/efectos
    // String url = "http://" + IP_ESTATICA.toString() + "/efectos";
    String url = "http://192.168.4.1/efectos";
    
    // 3. Agregar el parámetro 'track_index' a la consulta (Query Parameter)
    // El servidor usará este valor para saber qué reproducir.
    // String payload = "?track_index=" + String(trackIndex);
    String fullUrl = url ;//+ payload;

    Serial.println(">>> Enviando petición GET a: " + fullUrl);

    // 4. Inicializar y Configurar la petición
    HTTPClient http;
    http.begin(fullUrl); 

    // 5. Ejecutar la petición GET
    int httpResponseCode = http.GET();

    // 6. Procesar la Respuesta del Servidor
    if (httpResponseCode > 0) {
        // Código 200 (OK), 404 (No encontrado), 500 (Error de servidor), etc.
        Serial.printf("<<< Código HTTP recibido: %d\n", httpResponseCode);
        String response = http.getString();
        Serial.println("<<< Respuesta del servidor: " + response);
    } else {
        // Manejo de errores de conexión o timeout
        Serial.printf("<<< Error en la petición GET. Código: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    // 7. Finalizar y liberar recursos
    http.end();
}