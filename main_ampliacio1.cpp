#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Definición de pines
#define LED_SEGUNDOS 4      
#define LED_MODO 5          
#define BTN_MODO 16         
#define BTN_INCREMENTO 17   
#define BTN_FORMATO 18      // NOU BOTÓ: Para cambiar formato 12h/24h

// Variables para el reloj
volatile int horas = 0;
volatile int minutos = 0;
volatile int segundos = 0;
volatile int modo = 0; // 0: normal, 1: ajuste horas, 2: ajuste minutos
volatile bool formato24h = true; // NOVA VARIABLE: Controla el format

// Variables FreeRTOS
QueueHandle_t botonQueue;
SemaphoreHandle_t relojMutex;

// Estructura para los eventos de botones
typedef struct {
  uint8_t boton;
  uint32_t tiempo;
} EventoBoton;

// Prototipos de tareas
void TareaReloj(void *pvParameters);
void TareaLecturaBotones(void *pvParameters);
void TareaActualizacionDisplay(void *pvParameters);
void TareaControlLEDs(void *pvParameters);

// Función para manejar interrupciones de botones
void IRAM_ATTR ISR_Boton(void *arg) {
  // Ajustat el càsting per evitar warnings a l'ESP32-S3
  uint8_t numeroPulsador = (uint32_t)(uintptr_t)arg; 
  EventoBoton evento;
  evento.boton = numeroPulsador;
  evento.tiempo = xTaskGetTickCountFromISR();
  xQueueSendFromISR(botonQueue, &evento, NULL);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando Reloj Digital con RTOS - Ampliado 12/24h");

  // Configurar pines
  pinMode(LED_SEGUNDOS, OUTPUT);
  pinMode(LED_MODO, OUTPUT);
  pinMode(BTN_MODO, INPUT_PULLUP);
  pinMode(BTN_INCREMENTO, INPUT_PULLUP);
  pinMode(BTN_FORMATO, INPUT_PULLUP); 

  // Crear recursos RTOS
  botonQueue = xQueueCreate(10, sizeof(EventoBoton));
  relojMutex = xSemaphoreCreateMutex();

  // Configurar interrupciones para los botones
  attachInterruptArg(BTN_MODO, ISR_Boton, (void*)BTN_MODO, FALLING);
  attachInterruptArg(BTN_INCREMENTO, ISR_Boton, (void*)BTN_INCREMENTO, FALLING);
  attachInterruptArg(BTN_FORMATO, ISR_Boton, (void*)BTN_FORMATO, FALLING);

  // Crear tareas
  xTaskCreate(TareaReloj, "Reloj Task", 2048, NULL, 1, NULL);
  xTaskCreate(TareaLecturaBotones, "Botones Task", 2048, NULL, 2, NULL);
  xTaskCreate(TareaActualizacionDisplay, "Display Task", 2048, NULL, 1, NULL);
  xTaskCreate(TareaControlLEDs, "LEDsTask", 1024, NULL, 1, NULL);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}

void TareaReloj(void *pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(1000);
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
    if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
      if (modo == 0) {
        segundos++;
        if (segundos >= 60) {
          segundos = 0;
          minutos++;
          if (minutos >= 60) {
            minutos = 0;
            horas++;
            if (horas >= 24) horas = 0;
          }
        }
      }
      xSemaphoreGive(relojMutex);
    }
  }
}

void TareaLecturaBotones(void *pvParameters) {
  EventoBoton evento;
  uint32_t ultimoTiempoBoton = 0;
  const uint32_t debounceTime = pdMS_TO_TICKS(300);

  for (;;) {
    if (xQueueReceive(botonQueue, &evento, portMAX_DELAY) == pdPASS) {
      if ((evento.tiempo - ultimoTiempoBoton) >= debounceTime) {
        if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
          
          if (evento.boton == BTN_MODO) {
            modo = (modo + 1) % 3;
            Serial.printf("Cambio de modo: %d\n", modo);
          } 
          else if (evento.boton == BTN_INCREMENTO) {
            if (modo == 1) {
              horas = (horas + 1) % 24;
              Serial.printf("Horas ajustadas a: %d\n", horas);
            } else if (modo == 2) {
              minutos = (minutos + 1) % 60;
              segundos = 0;
              Serial.printf("Minutos ajustados a: %d\n", minutos);
            }
          }
          else if (evento.boton == BTN_FORMATO) {
            formato24h = !formato24h;
            Serial.println(formato24h ? "Format: 24h" : "Format: 12h");
          }
          
          xSemaphoreGive(relojMutex);
        }
        ultimoTiempoBoton = evento.tiempo;
      }
    }
  }
}

void TareaActualizacionDisplay(void *pvParameters) {
  int horasAnterior = -1, minutosAnterior = -1, segundosAnterior = -1, modoAnterior = -1;
  bool formatoAnterior = !formato24h;

  for (;;) {
    if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
      bool cambios = (horas != horasAnterior) || (minutos != minutosAnterior) || 
                     (segundos != segundosAnterior) || (modo != modoAnterior) || 
                     (formato24h != formatoAnterior);

      if (cambios) {
        if (formato24h) {
          Serial.printf("%02d:%02d:%02d", horas, minutos, segundos);
        } else {
          int horas12 = horas % 12;
          if (horas12 == 0) horas12 = 12;
          String ampm = (horas < 12) ? " AM" : " PM";
          Serial.printf("%02d:%02d:%02d%s", horas12, minutos, segundos, ampm.c_str());
        }

        if (modo == 0) Serial.println(" [Modo Normal]");
        else if (modo == 1) Serial.println(" [Ajuste Horas]");
        else if (modo == 2) Serial.println(" [Ajuste Minutos]");

        horasAnterior = horas;
        minutosAnterior = minutos;
        segundosAnterior = segundos;
        modoAnterior = modo;
        formatoAnterior = formato24h;
      }
      xSemaphoreGive(relojMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void TareaControlLEDs(void *pvParameters) {
  bool estadoLedSegundos = false;
  for (;;) {
    if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
      estadoLedSegundos = !estadoLedSegundos;
      digitalWrite(LED_SEGUNDOS, estadoLedSegundos);
      digitalWrite(LED_MODO, modo > 0);
      xSemaphoreGive(relojMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}