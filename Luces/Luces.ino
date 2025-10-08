// --- Definiciones de pines ---
const int ZERO_CROSS_PIN = D1; // Pin donde el ESP8266 recibe la señal del sensor de cruce por cero
const int TRIAC_GATE_PIN = D2; // Pin donde el ESP8266 envía el pulso para disparar el TRIAC

// --- Variables de control ---
volatile int  dimmerValue = 0;      // 0 = apagado, 100 = 100% potencia
volatile bool zeroCrossDetected = false; // Bandera para indicar cruce por cero

// --- Constantes de tiempo (para 50Hz, ajustar para 60Hz) ---
// Período de un semiciclo: 10,000 microsegundos (1/2 * 1/50Hz = 0.01s = 10ms)
const unsigned long SEMI_CYCLE_PERIOD_US = 10000; 

// Microsegundos para el delay de disparo del TRIAC para 100% de potencia (casi inmediato después del cruce)
// y 0% de potencia (al final del semiciclo)
// El rango real es de unos 0us (casi 100%) a ~9000us (casi 0%) para 50Hz.
// Esto es solo un ejemplo, los valores exactos dependen de tu módulo.
const unsigned long MIN_DIM_DELAY_US = 50;  // Retraso mínimo (casi 100% potencia)
const unsigned long MAX_DIM_DELAY_US = 9000; // Retraso máximo (casi 0% potencia)


// --- Función de Interrupción para el Cruce por Cero (ISR) ---
// Se ejecuta cada vez que el ESP8266 detecta un flanco ascendente (RISING) o descendente (FALLING)
// en el pin ZERO_CROSS_PIN. Debes saber si tu sensor da un pulso ALTO o BAJO al cruzar por cero.
// ICACHE_RAM_ATTR es crucial para las ISRs en ESP8266.
void ICACHE_RAM_ATTR zeroCrossISR() {
  zeroCrossDetected = true; // Establecemos la bandera
}

// --- Función para disparar el TRIAC ---
void ICACHE_RAM_ATTR fireTriac() {
  digitalWrite(TRIAC_GATE_PIN, HIGH); // Envía un pulso ALTO al TRIAC
  delayMicroseconds(50);              // Mantiene el pulso por un tiempo muy corto (50us es común)
  digitalWrite(TRIAC_GATE_PIN, LOW);  // Apaga el pulso
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n--- Control de Dimmer Manual ---"));

  pinMode(ZERO_CROSS_PIN, INPUT_PULLUP); // El sensor de cruce por cero suele ser un open-collector o necesita pull-up
  pinMode(TRIAC_GATE_PIN, OUTPUT);
  digitalWrite(TRIAC_GATE_PIN, LOW); // Asegura que el TRIAC esté apagado inicialmente

  // Adjuntar la interrupción al pin de cruce por cero
  // RISING si el pulso de cruce por cero pasa de BAJO a ALTO
  // FALLING si el pulso de cruce por cero pasa de ALTO a BAJO
  // CHANGE si el pulso simplemente cambia de estado al cruzar por cero (detecta ambos flancos)
  attachInterrupt(digitalPinToInterrupt(ZERO_CROSS_PIN), zeroCrossISR, RISING); 

  Serial.println(F("Dimmer listo. Ajustando potencia..."));
  dimmerValue = 0; // Inicia apagado
}

void loop() {
  static unsigned long lastChangeTime = 0;
  const unsigned long CHANGE_INTERVAL = 1000; // Cambiar estado cada 1 segundo

  // Lógica principal para cambiar la potencia de la luz cada segundo
  if (millis() - lastChangeTime >= CHANGE_INTERVAL) {
    lastChangeTime = millis();
    if (dimmerValue == 0) {
      dimmerValue = 80; // Encender al 80%
      Serial.println("Luz: Encendida (80%)");
    } else {
      dimmerValue = 0; // Apagar
      Serial.println("Luz: Apagada (0%)");
    }
  }

  // --- Lógica del Dimmer ---
  if (zeroCrossDetected) {
    zeroCrossDetected = false; // Resetear la bandera

    // Calcular el retraso en microsegundos basándose en dimmerValue (0-100)
    // Mapeamos 0-100 a MAX_DIM_DELAY_US - MIN_DIM_DELAY_US
    // dimmerValue 0 (apagado)  -> MAX_DIM_DELAY_US
    // dimmerValue 100 (encendido) -> MIN_DIM_DELAY_US
    unsigned long delayUs;
    if (dimmerValue == 0) {
      delayUs = MAX_DIM_DELAY_US; // Apagado total (disparar casi al final del semiciclo)
    } else if (dimmerValue == 100) {
      delayUs = MIN_DIM_DELAY_US; // Encendido total (disparar casi al principio)
    } else {
      // Cálculo lineal: (100 - dimmerValue) / 100 * (MAX_DIM_DELAY_US - MIN_DIM_DELAY_US) + MIN_DIM_DELAY_US
      delayUs = map(100 - dimmerValue, 0, 100, MIN_DIM_DELAY_US, MAX_DIM_DELAY_US);
    }
    
    // Esperar el tiempo de retardo antes de disparar el TRIAC
    delayMicroseconds(delayUs);
    fireTriac(); // Disparar el TRIAC
  }
  
  // Pequeño delay para evitar que el loop se ejecute demasiado rápido
  // si no hay cruces por cero (ej. al arrancar).
  // Es importante que el loop no tarde demasiado en reaccionar al zeroCrossDetected.
  // Un delay(1) o delay(0) podría ser suficiente.
  // Si la ISR se dispara muy rápido, podría perderse la flag en algunos casos,
  // pero el ESP8266 es rápido.
  delay(1); 
}