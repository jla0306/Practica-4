#include <Arduino.h>

// Definim el pin del LED intern de l'ESP32
const int ledPin = 4; 

// Creem els handles per als nostres semàfors
SemaphoreHandle_t semaforEncendre;
SemaphoreHandle_t semaforApagar;

// Declaració de les funcions
void tascaEncendreLED(void * parameter);
void tascaApagarLED(void * parameter);

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // 1. Creem els semàfors binaris
  semaforEncendre = xSemaphoreCreateBinary();
  semaforApagar = xSemaphoreCreateBinary();

  // 2. Iniciem el procés donant el primer "testimoni" a la tasca d'encendre
  xSemaphoreGive(semaforEncendre);

  // 3. Creem la tasca que encendrà el LED
  xTaskCreate(
    tascaEncendreLED,   // Funció de la tasca
    "Tasca Encendre",   // Nom de la tasca per depurar
    1000,               // Mida de la pila
    NULL,               // Paràmetres
    1,                  // Prioritat (les dues tenen prioritat 1)
    NULL                // Handle de la tasca (no en necessitem per ara)
  );

  // 4. Creem la tasca que apagarà el LED
  xTaskCreate(
    tascaApagarLED,     // Funció de la tasca
    "Tasca Apagar",     // Nom de la tasca per depurar
    1000,               // Mida de la pila
    NULL,               // Paràmetres
    1,                  // Prioritat
    NULL                // Handle de la tasca
  );
}

void loop() {
  // Com que estem treballant amb tasques customitzades, eliminem la tasca del loop 
  // perquè no consumeixi recursos innecessaris de l'ESP32.
  vTaskDelete(NULL); 
}

// --- Definició de la Tasca 1: Encén el LED ---
void tascaEncendreLED(void * parameter) {
  for(;;) {
    // Intentem agafar el semàfor d'encendre. La tasca es bloqueja aquí si no el té.
    if (xSemaphoreTake(semaforEncendre, portMAX_DELAY) == pdTRUE) {
      digitalWrite(ledPin, HIGH);
      Serial.println("LED ENCES");
      
      // La tasca s'atura mig segon (com un delay(), però permetent altres tasques)
      vTaskDelay(500 / portTICK_PERIOD_MS); 
      
      // Passem el testimoni a la tasca d'apagar
      xSemaphoreGive(semaforApagar);
    }
  }
}

// --- Definició de la Tasca 2: Apaga el LED ---
void tascaApagarLED(void * parameter) {
  for(;;) {
    // Intentem agafar el semàfor d'apagar. Es bloqueja esperant que l'altra tasca l'alliberi.
    if (xSemaphoreTake(semaforApagar, portMAX_DELAY) == pdTRUE) {
      digitalWrite(ledPin, LOW);
      Serial.println("LED APAGAT");
      
      // La tasca s'atura mig segon
      vTaskDelay(500 / portTICK_PERIOD_MS);
      
      // Tornem a passar el testimoni a la tasca d'encendre (cicle complet)
      xSemaphoreGive(semaforEncendre);
    }
  }
}