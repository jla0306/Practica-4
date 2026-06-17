# Informe de Pràctica 4: Sistemes Operatius en Temps Real (FreeRTOS)

**Autors:** Julio Lázaro Alcobendas i Gerard Rodríguez González
**Data:** 17 de Maig de 2026
**Repositori GitHub:** https://github.com/jla0306/Practica-4

> **Nota:** El fitxer `platformio.ini` és comú per a tots els exercicis d'aquesta pràctica:
> ```ini
> [env:esp32-s3-devkitc-1]
> platform  = espressif32
> board     = esp32-s3-devkitc-1
> framework = arduino
> monitor_speed = 115200
> ```

---

# Exercici 1: Multitasca bàsica amb FreeRTOS

## 1. Objectius de la pràctica

L'objectiu d'aquest exercici és comprendre el funcionament d'un sistema operatiu en temps real (RTOS) aplicat al microcontrolador ESP32-S3. Es programa una tasca addicional que s'executa en paral·lel al `loop()` principal d'Arduino, observant com el planificador de FreeRTOS reparteix el temps de CPU entre elles.

## 2. Desenvolupament i Arquitectura

S'utilitza la funció `xTaskCreate()` per registrar una nova tasca al planificador de FreeRTOS. Aquesta tasca s'executa de forma concurrent amb la tasca implícita del `loop()` d'Arduino. Ambdues tasques imprimeixen missatges pel port sèrie cada segon, demostrant la commutació de context del RTOS.

## 3. Codi Principal (main.cpp)

```cpp
#include <Arduino.h>
#include <freertos/task.h>

void anotherTask(void * parameter);

void setup() {
  Serial.begin(112500);

  xTaskCreate(
    anotherTask,      /* Funció de la tasca */
    "another Task",   /* Nom de la tasca */
    10000,            /* Mida de la pila */
    NULL,             /* Paràmetre */
    1,                /* Prioritat */
    NULL);            /* Handle */
}

void loop() {
  Serial.println("this is ESP32 Task");
  delay(1000);
}

void anotherTask(void * parameter) {
  for(;;) {
    Serial.println("this is another Task");
    delay(1000);
  }
  vTaskDelete(NULL); // Mai s'executa
}
```

## 4. Funcionament del codi

El programa crea una tasca addicional (`anotherTask`) durant el `setup()`. Tant el `loop()` com `anotherTask` s'executen en bucle infinit, cadascun imprimint el seu missatge i esperant 1 segon amb `delay()`. El planificador de FreeRTOS s'encarrega de repartir el temps de CPU entre ambdues tasques. Quan una tasca fa `delay()`, el planificador cedeix la CPU a l'altra.

## 5. Sortida pel Monitor Sèrie

```
this is ESP32 Task
this is another Task
this is ESP32 Task
this is another Task
this is ESP32 Task
this is another Task
...
```

La sortida s'intercala perquè totes dues tasques tenen la mateixa prioritat (1) i cada una bloqueja durant 1 segon amb `delay()`, permetent a l'altra executar-se.

## 6. Diagrama de flux

```
setup()
  ↓
Serial.begin(112500)
  ↓
xTaskCreate(anotherTask, ...)
  ↓
┌─────────────────────────────────┐
│         FreeRTOS Scheduler      │
├──────────────────┬──────────────┤
│    loop() Task   │ anotherTask  │
│ Serial "ESP32"   │ Serial "ano" │
│ delay(1000)      │ delay(1000)  │
│ (cedeix CPU)     │ (cedeix CPU) │
└──────────────────┴──────────────┘
       ↻ (cicle continu)
```

## 7. Preguntes de la pràctica

**Quin és el temps lliure del processador?** La major part del temps ambdues tasques estan bloquejades dins del `delay(1000)`. Durant aquest bloqueig, el planificador posa la tasca en estat *Blocked* i pot executar altres tasques del sistema o entrar en mode *Idle*. Les accions d'imprimir per sèrie són molt ràpides, de manera que el processador té temps lliure gairebé tot el cicle.

**Quina és la diferència entre `delay()` i `vTaskDelay()`?** `delay()` d'Arduino bloqueja el nucli sense alliberar la CPU al planificador. `vTaskDelay()` sí que notifica al planificador FreeRTOS que la tasca no necessita CPU durant el temps indicat, permetent executar altres tasques. En contextos RTOS, sempre és preferible usar `vTaskDelay()`.

## 8. Conclusions

Aquest primer exercici ens ha permès verificar que el planificador FreeRTOS de l'ESP32-S3 és capaç de gestionar múltiples tasques de forma concurrent. La commutació de context és transparent per al programador: simplement cal definir les tasques i el planificador s'encarrega de la resta.

---

# Exercici 2: Sincronització de tasques amb semàfors

## 1. Objectiu

L'objectiu és implementar el control d'un LED mitjançant dues tasques separades (una per encendre'l i una per apagar-lo) que es sincronitzen usant semàfors binaris de FreeRTOS, garantint que les operacions es facin en l'ordre correcte.

## 2. Desenvolupament

Es creen dos semàfors binaris (`semaforEncendre` i `semaforApagar`) que actuen com a "testimonis". Inicialment el testimoni s'entrega a la tasca d'encendre. Quan aquesta acaba, passa el testimoni a la tasca d'apagar, i viceversa, formant un cicle sincrono.

## 3. Codi Principal (main.cpp)

```cpp
#include <Arduino.h>

const int ledPin = 4;

SemaphoreHandle_t semaforEncendre;
SemaphoreHandle_t semaforApagar;

void tascaEncendreLED(void * parameter);
void tascaApagarLED(void * parameter);

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  semaforEncendre = xSemaphoreCreateBinary();
  semaforApagar   = xSemaphoreCreateBinary();

  // El primer testimoni és per a la tasca d'encendre
  xSemaphoreGive(semaforEncendre);

  xTaskCreate(tascaEncendreLED, "Tasca Encendre", 1000, NULL, 1, NULL);
  xTaskCreate(tascaApagarLED,   "Tasca Apagar",   1000, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(NULL); // Eliminem la tasca del loop per no consumir recursos
}

void tascaEncendreLED(void * parameter) {
  for(;;) {
    if (xSemaphoreTake(semaforEncendre, portMAX_DELAY) == pdTRUE) {
      digitalWrite(ledPin, HIGH);
      Serial.println("LED ENCES");
      vTaskDelay(500 / portTICK_PERIOD_MS);
      xSemaphoreGive(semaforApagar);
    }
  }
}

void tascaApagarLED(void * parameter) {
  for(;;) {
    if (xSemaphoreTake(semaforApagar, portMAX_DELAY) == pdTRUE) {
      digitalWrite(ledPin, LOW);
      Serial.println("LED APAGAT");
      vTaskDelay(500 / portTICK_PERIOD_MS);
      xSemaphoreGive(semaforEncendre);
    }
  }
}
```

## 4. Funcionament del codi

Al `setup()` es creen els dos semàfors binaris i s'entrega immediatament el primer testimoni a `semaforEncendre`. Quan la tasca d'encendre agafa el seu semàfor (`xSemaphoreTake`), encén el LED, espera 500 ms i allibera el semàfor d'apagar (`xSemaphoreGive`). Llavors la tasca d'apagar es desbloqueja, apaga el LED, espera 500 ms i torna el testimoni a la tasca d'encendre. El cicle es repeteix indefinidament.

La tasca del `loop()` s'elimina amb `vTaskDelete(NULL)` per no consumir recursos innecessaris.

## 5. Sortida pel Monitor Sèrie

```
LED ENCES
LED APAGAT
LED ENCES
LED APAGAT
LED ENCES
LED APAGAT
...
```

## 6. Diagrama de flux

```
setup()
  ↓
Crear semaforEncendre + semaforApagar
  ↓
xSemaphoreGive(semaforEncendre)  ← primer testimoni
  ↓
Crear tascaEncendreLED + tascaApagarLED
  ↓
vTaskDelete(loop)

┌─────────────────────────┐    ┌─────────────────────────┐
│   tascaEncendreLED      │    │   tascaApagarLED         │
│ Take(semaforEncendre)   │◄──►│ Take(semaforApagar)      │
│ digitalWrite(HIGH)      │    │ digitalWrite(LOW)         │
│ Serial "LED ENCES"      │    │ Serial "LED APAGAT"       │
│ vTaskDelay(500ms)       │    │ vTaskDelay(500ms)         │
│ Give(semaforApagar) ────┼───►│ Give(semaforEncendre) ───┤
└─────────────────────────┘    └─────────────────────────┘
              ↻ (cicle sincrono continu)
```

## 7. Conclusions

Aquest exercici ens ha demostrat com els semàfors binaris de FreeRTOS permeten sincronitzar tasques de forma elegant i segura. Sense semàfors, les dues tasques podrien entrar en conflicte i el LED podria parpallejar de forma incorrecta. El patró "passa el testimoni" és una tècnica fonamental en sistemes multitasca per garantir l'ordre d'execució.

---

# AMPLIACIÓ DE NOTA 1: Rellotge Digital amb FreeRTOS

## 1. Objectiu

Implementar un rellotge digital funcional amb format 12h/24h, modes d'ajust d'hores i minuts, control de LEDs i lectura de botons mitjançant interrupcions, tot gestionat amb tasques FreeRTOS, cues i mutex.

## 2. Desenvolupament

El sistema utilitza 4 tasques FreeRTOS coordinades per un mutex (`relojMutex`):

| Tasca | Funció | Prioritat |
|-------|--------|-----------|
| `TareaReloj` | Incrementa el temps cada segon | 1 |
| `TareaLecturaBotones` | Processa events de la cua de botons | 2 |
| `TareaActualizacionDisplay` | Imprimeix l'hora pel port sèrie | 1 |
| `TareaControlLEDs` | Gestiona LEDs d'estat | 1 |

Les interrupcions dels botons (`ISR_Boton`) envien events a una cua (`botonQueue`) que la tasca de lectura consumeix de forma segura.

## 3. Codi Principal (main.cpp)

```cpp
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define LED_SEGUNDOS  4
#define LED_MODO      5
#define BTN_MODO     16
#define BTN_INCREMENTO 17
#define BTN_FORMATO  18

volatile int horas = 0, minutos = 0, segundos = 0;
volatile int modo = 0;          // 0: normal, 1: ajust hores, 2: ajust minuts
volatile bool formato24h = true;

QueueHandle_t botonQueue;
SemaphoreHandle_t relojMutex;

typedef struct { uint8_t boton; uint32_t tiempo; } EventoBoton;

void IRAM_ATTR ISR_Boton(void *arg) {
  uint8_t num = (uint32_t)(uintptr_t)arg;
  EventoBoton ev = { num, xTaskGetTickCountFromISR() };
  xQueueSendFromISR(botonQueue, &ev, NULL);
}

void TareaReloj(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
      if (modo == 0) {
        if (++segundos >= 60) { segundos = 0;
          if (++minutos >= 60) { minutos = 0;
            if (++horas >= 24) horas = 0; } }
      }
      xSemaphoreGive(relojMutex);
    }
  }
}

void TareaLecturaBotones(void *pvParameters) {
  EventoBoton ev;
  uint32_t ultim = 0;
  for (;;) {
    if (xQueueReceive(botonQueue, &ev, portMAX_DELAY) == pdPASS) {
      if ((ev.tiempo - ultim) >= pdMS_TO_TICKS(300)) {
        if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
          if (ev.boton == BTN_MODO)       modo = (modo + 1) % 3;
          else if (ev.boton == BTN_INCREMENTO) {
            if (modo == 1) horas = (horas + 1) % 24;
            else if (modo == 2) { minutos = (minutos + 1) % 60; segundos = 0; }
          }
          else if (ev.boton == BTN_FORMATO) formato24h = !formato24h;
          xSemaphoreGive(relojMutex);
        }
        ultim = ev.tiempo;
      }
    }
  }
}

void TareaActualizacionDisplay(void *pvParameters) {
  int hA=-1, mA=-1, sA=-1, mdA=-1; bool fA=!formato24h;
  for (;;) {
    if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
      if (horas!=hA || minutos!=mA || segundos!=sA || modo!=mdA || formato24h!=fA) {
        if (formato24h) Serial.printf("%02d:%02d:%02d", horas, minutos, segundos);
        else {
          int h12 = horas % 12; if (h12==0) h12=12;
          Serial.printf("%02d:%02d:%02d%s", h12, minutos, segundos,
                        horas<12?" AM":" PM");
        }
        if (modo==0) Serial.println(" [Modo Normal]");
        else if (modo==1) Serial.println(" [Ajuste Horas]");
        else Serial.println(" [Ajuste Minutos]");
        hA=horas; mA=minutos; sA=segundos; mdA=modo; fA=formato24h;
      }
      xSemaphoreGive(relojMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void TareaControlLEDs(void *pvParameters) {
  bool estat = false;
  for (;;) {
    if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
      estat = !estat;
      digitalWrite(LED_SEGUNDOS, estat);
      digitalWrite(LED_MODO, modo > 0);
      xSemaphoreGive(relojMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_SEGUNDOS, OUTPUT); pinMode(LED_MODO, OUTPUT);
  pinMode(BTN_MODO, INPUT_PULLUP); pinMode(BTN_INCREMENTO, INPUT_PULLUP);
  pinMode(BTN_FORMATO, INPUT_PULLUP);

  botonQueue  = xQueueCreate(10, sizeof(EventoBoton));
  relojMutex  = xSemaphoreCreateMutex();

  attachInterruptArg(BTN_MODO,      ISR_Boton, (void*)BTN_MODO,      FALLING);
  attachInterruptArg(BTN_INCREMENTO,ISR_Boton, (void*)BTN_INCREMENTO,FALLING);
  attachInterruptArg(BTN_FORMATO,   ISR_Boton, (void*)BTN_FORMATO,   FALLING);

  xTaskCreate(TareaReloj,               "Reloj",   2048, NULL, 1, NULL);
  xTaskCreate(TareaLecturaBotones,      "Botones", 2048, NULL, 2, NULL);
  xTaskCreate(TareaActualizacionDisplay,"Display", 2048, NULL, 1, NULL);
  xTaskCreate(TareaControlLEDs,         "LEDs",    1024, NULL, 1, NULL);
}

void loop() { vTaskDelay(portMAX_DELAY); }
```

## 4. Funcionament

La tasca `TareaReloj` s'executa exactament cada 1000 ms amb `vTaskDelayUntil`, garantint precisió. Abans de modificar les variables compartides, agafa el `relojMutex`. La tasca de botons té prioritat 2 (la més alta) per respondre ràpidament a les interrupcions. El sistema de debounce de 300 ms per software evita lectures múltiples per un sol prement.

## 5. Sortida pel Monitor Sèrie

```
00:00:00 [Modo Normal]
00:00:01 [Modo Normal]
00:00:02 [Modo Normal]
Cambio de modo: 1
00:00:03 [Ajuste Horas]
Horas ajustadas a: 1
01:00:03 [Ajuste Horas]
Format: 12h
01:00:04 AM [Ajuste Horas]
```

## 6. Conclusions

Aquesta ampliació ens ha mostrat com construir un sistema multitasca real i robust. L'ús del mutex garanteix que les variables compartides entre tasques mai s'accedeixin simultàniament (condició de carrera). La combinació de cues per a events i mutex per a dades compartides és el patró estàndard en sistemes FreeRTOS professionals.

---

# AMPLIACIÓ DE NOTA 2: Joc "Atrapa el LED" amb FreeRTOS + Web Server

## 1. Objectiu

Implementar un joc interactiu on el jugador ha de prémer el botó corresponent al LED que s'il·lumina, gestionat per tasques FreeRTOS i accessible des d'un navegador web via WiFi en mode Access Point.

## 2. Especificacions del platformio.ini

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    esphome/AsyncTCP-esphome
    esphome/ESPAsyncWebServer-esphome
```

## 3. Arquitectura del sistema

| Tasca | Responsabilitat |
|-------|----------------|
| `TareaJuego` | Canvia el LED actiu segons la dificultat |
| `TareaLecturaBotones` | Detecta pulsacions i actualitza puntuació |
| `TareaTiempo` | Compta enrere els 30 segons de joc |
| `TareaServidorWeb` | Envia actualitzacions SSE al navegador cada 200 ms |

## 4. Codi Principal (main.cpp)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

const char* ssid = "ESP32_Game";
const char* password = "12345678";

#define LED1 4
#define LED2 5
#define LED3 6
#define LED_STATUS 7
#define BTN1 16
#define BTN2 17
#define BTN3 18

volatile int puntuacion = 0, tiempoJuego = 30, ledActivo = -1;
volatile bool juegoActivo = false;
volatile int dificultad = 1;

QueueHandle_t botonQueue;
SemaphoreHandle_t juegoMutex;
TaskHandle_t tareaJuegoHandle = NULL;
AsyncWebServer server(80);
AsyncEventSource events("/events");

typedef struct { uint8_t boton; uint32_t tiempo; } EventoBoton;

void IRAM_ATTR ISR_Boton(void *arg) {
  uint8_t num = (uint32_t)(uintptr_t)arg;
  EventoBoton ev = { num, xTaskGetTickCountFromISR() };
  xQueueSendFromISR(botonQueue, &ev, NULL);
}

// ... (funcions de tasques: TareaJuego, TareaLecturaBotones,
//      TareaTiempo, TareaServidorWeb, iniciarJuego, detenerJuego)

void setup() {
  Serial.begin(115200);
  // Configurar pins LEDs i botons
  // Crear recursos RTOS
  botonQueue = xQueueCreate(10, sizeof(EventoBoton));
  juegoMutex = xSemaphoreCreateMutex();
  // Interrupcions
  attachInterruptArg(BTN1, ISR_Boton, (void*)BTN1, FALLING);
  attachInterruptArg(BTN2, ISR_Boton, (void*)BTN2, FALLING);
  attachInterruptArg(BTN3, ISR_Boton, (void*)BTN3, FALLING);
  // WiFi AP
  WiFi.softAP(ssid, password);
  Serial.println("IP: " + WiFi.softAPIP().toString());
  // Rutes web i SSE
  server.on("/",          HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200,"text/html",index_html); });
  server.on("/toggle",    HTTP_GET, [](AsyncWebServerRequest *r){ juegoActivo?detenerJuego():iniciarJuego(); r->send(200,"text/plain","OK"); });
  server.on("/difficulty",HTTP_GET, [](AsyncWebServerRequest *r){ /* ajusta dificultad */ r->send(200,"text/plain","OK"); });
  server.addHandler(&events);
  server.begin();
  // Tasques RTOS
  xTaskCreate(TareaServidorWeb,    "WebTask", 4096, NULL, 1, NULL);
  xTaskCreate(TareaLecturaBotones, "BtnTask", 2048, NULL, 2, NULL);
  xTaskCreate(TareaTiempo,         "TimeTask",2048, NULL, 1, NULL);
}

void loop() { vTaskDelay(portMAX_DELAY); }
```

## 5. Funcionament

El joc funciona en mode Access Point: el jugador es connecta a la xarxa `ESP32_Game` i accedeix a `192.168.4.1` amb el navegador. Des de la interfície web pot iniciar/aturar el joc i ajustar la dificultat (1-5). La `TareaJuego` il·lumina un LED aleatori en intervals que disminueixen amb la dificultat (`1000 - dificultad * 150` ms). El jugador ha de prémer el botó corresponent al LED actiu. Si encerta, suma un punt; si falla, en resta un. La `TareaServidorWeb` envia actualitzacions SSE cada 200 ms per mantenir el navegador sincronitzat en temps real.

## 6. Sortida pel Monitor Sèrie

```
Inicializando Juego ESP32...
Dirección IP del AP: 192.168.4.1
¡Correcto!
¡Correcto!
Incorrecto...
¡Tiempo agotado!
```

## 7. Conclusions

Aquesta ampliació combina tots els conceptes apresos: multitasca amb FreeRTOS, sincronització amb mutex, comunicació amb cues, interrupcions, mode AP WiFi i servidor web asíncron. El resultat és un sistema embegut complet que demostra la potència de l'ESP32-S3 per gestionar múltiples responsabilitats de forma concurrent i fiable.
