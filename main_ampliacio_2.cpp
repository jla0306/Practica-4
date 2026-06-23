#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Configuración de red WiFi
const char* ssid = "ESP32_Game"; // Nombre de la red WiFi
const char* password = "12345678"; // Contraseña

// Definición de pines (Adaptados para ESP32-S3)
#define LED1 4
#define LED2 5
#define LED3 6
#define LED_STATUS 7 // LED de estado del juego
#define BTN1 16
#define BTN2 17
#define BTN3 18

// Variables del juego
volatile int puntuacion = 0;
volatile int tiempoJuego = 30;
volatile int ledActivo = -1;
volatile bool juegoActivo = false;
volatile int dificultad = 1;

// Variables FreeRTOS
QueueHandle_t botonQueue;
SemaphoreHandle_t juegoMutex;
TaskHandle_t tareaJuegoHandle = NULL;

// Servidor web
AsyncWebServer server(80);
AsyncEventSource events("/events");

// Estructura para los eventos
typedef struct {
  uint8_t boton;
  uint32_t tiempo;
} EventoBoton;

// Prototipos
void TareaServidorWeb(void *pvParameters);
void TareaJuego(void *pvParameters);
void TareaLecturaBotones(void *pvParameters);
void TareaTiempo(void *pvParameters);
void iniciarJuego();
void detenerJuego();
void desactivarTodosLEDs();
String obtenerEstadoJuego();
void enviarActualizacionWeb();

// ISR Corregida para ESP32-S3
void IRAM_ATTR ISR_Boton(void *arg) {
  uint8_t numeroPulsador = (uint32_t)(uintptr_t)arg;
  EventoBoton evento;
  evento.boton = numeroPulsador;
  evento.tiempo = xTaskGetTickCountFromISR();
  xQueueSendFromISR(botonQueue, &evento, NULL);
}

// HTML para la página principal
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Game - Atrapa el LED</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin:0px auto; padding: 20px; }
    .container { display: flex; flex-direction: column; width: 100%; max-width: 500px; margin: 0 auto; }
    .card { background-color: #F8F7F9; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); padding: 20px; margin: 10px 0; border-radius: 5px; }
    .button { padding: 15px 50px; font-size: 24px; text-align: center; color: #fff; background-color: #0f8b8d; border: none; border-radius: 5px; cursor: pointer; }
    .button:active { transform: translateY(2px); }
    .progress-container { width: 100%; background-color: #ddd; border-radius: 5px; margin: 10px 0; }
    .progress-bar { height: 30px; background-color: #04AA6D; border-radius: 5px; width: 100%; text-align: center; line-height: 30px; color: white; }
    .led-container { display: flex; justify-content: space-around; margin: 20px 0; }
    .led { width: 50px; height: 50px; border-radius: 50%; background-color: #333; opacity: 0.3; }
    #led1 { background-color: #ff0000; }
    #led2 { background-color: #00ff00; }
    #led3 { background-color: #0000ff; }
    .instructions { text-align: left; }
  </style>
</head>
<body>
  <div class="container">
    <h2>ESP32 Game - Atrapa el LED</h2>
    <div class="card">
      <h3>Panel de Control</h3>
      <p>Puntuación: <span id="score">0</span></p>
      <p>Tiempo: <span id="time">30</span> segundos</p>
      <div class="progress-container"><div id="progress" class="progress-bar">30s</div></div>
      <div class="difficulty">
        <label>Dificultad: <span id="diffValue">1</span></label>
        <input type="range" min="1" max="5" value="1" id="difficultyRange">
      </div>
      <button id="startBtn" class="button">Iniciar Juego</button>
    </div>
    <div class="card">
      <h3>Estado del Juego</h3>
      <div class="led-container">
        <div class="led" id="led1"></div>
        <div class="led" id="led2"></div>
        <div class="led" id="led3"></div>
      </div>
      <p id="gameStatus">Juego detenido</p>
    </div>
  </div>
  <script>
    var gateway = `ws://${window.location.hostname}/events`;
    var source;
    window.addEventListener('load', function() {
      source = new EventSource('/events');
      source.addEventListener('update', function(e) {
        var data = JSON.parse(e.data);
        document.getElementById('score').innerHTML = data.score;
        document.getElementById('time').innerHTML = data.time;
        document.getElementById('progress').style.width = (data.time/30*100) + '%';
        document.getElementById('progress').innerHTML = data.time + 's';
        document.getElementById('gameStatus').innerHTML = data.active ? "Juego en curso" : "Juego detenido";
        document.getElementById('led1').style.opacity = (data.led === 0) ? "1.0" : "0.3";
        document.getElementById('led2').style.opacity = (data.led === 1) ? "1.0" : "0.3";
        document.getElementById('led3').style.opacity = (data.led === 2) ? "1.0" : "0.3";
        document.getElementById('startBtn').innerHTML = data.active ? "Detener Juego" : "Iniciar Juego";
      }, false);
      
      document.getElementById('startBtn').addEventListener('click', function() {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/toggle", true);
        xhr.send();
      });
      
      document.getElementById('difficultyRange').addEventListener('change', function() {
        var diff = this.value;
        document.getElementById('diffValue').innerHTML = diff;
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/difficulty?value=" + diff, true);
        xhr.send();
      });
    });
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando Juego ESP32...");

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);

  desactivarTodosLEDs();
  digitalWrite(LED_STATUS, LOW);

  botonQueue = xQueueCreate(10, sizeof(EventoBoton));
  juegoMutex = xSemaphoreCreateMutex();

  attachInterruptArg(BTN1, ISR_Boton, (void*)BTN1, FALLING);
  attachInterruptArg(BTN2, ISR_Boton, (void*)BTN2, FALLING);
  attachInterruptArg(BTN3, ISR_Boton, (void*)BTN3, FALLING);

  // Configurar WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("Dirección IP del AP: ");
  Serial.println(WiFi.softAPIP());

  // Rutas web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    if (juegoActivo) detenerJuego();
    else iniciarJuego();
    request->send(200, "text/plain", "OK");
  });

  server.on("/difficulty", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      int valor = request->getParam("value")->value().toInt();
      if (valor >= 1 && valor <= 5) {
        if (xSemaphoreTake(juegoMutex, portMAX_DELAY) == pdTRUE) {
          dificultad = valor;
          xSemaphoreGive(juegoMutex);
        }
      }
    }
    request->send(200, "text/plain", "OK");
  });

  events.onConnect([](AsyncEventSourceClient *client){
    client->send(obtenerEstadoJuego().c_str(), "update", millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();

  // Tareas RTOS
  xTaskCreate(TareaServidorWeb, "WebTask", 4096, NULL, 1, NULL);
  xTaskCreate(TareaLecturaBotones, "BtnTask", 2048, NULL, 2, NULL);
  xTaskCreate(TareaTiempo, "TimeTask", 2048, NULL, 1, NULL);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}

void TareaServidorWeb(void *pvParameters) {
  for (;;) {
    enviarActualizacionWeb();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void TareaJuego(void *pvParameters) {
  int ultimoLed = -1;
  for (;;) {
    int espera = 1000;
    if (xSemaphoreTake(juegoMutex, portMAX_DELAY) == pdTRUE) {
      if (juegoActivo) {
        int nuevoLed;
        do { nuevoLed = random(0, 3); } while (nuevoLed == ultimoLed);
        ledActivo = nuevoLed;
        ultimoLed = nuevoLed;
        
        desactivarTodosLEDs();
        if (ledActivo == 0) digitalWrite(LED1, HIGH);
        else if (ledActivo == 1) digitalWrite(LED2, HIGH);
        else if (ledActivo == 2) digitalWrite(LED3, HIGH);
        
        espera = 1000 - (dificultad * 150);
      }
      xSemaphoreGive(juegoMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(espera));
  }
}

void TareaLecturaBotones(void *pvParameters) {
  EventoBoton evento;
  uint32_t ultimoTiempo = 0;
  for (;;) {
    if (xQueueReceive(botonQueue, &evento, portMAX_DELAY) == pdPASS) {
      if ((evento.tiempo - ultimoTiempo) >= pdMS_TO_TICKS(200)) {
        if (xSemaphoreTake(juegoMutex, portMAX_DELAY) == pdTRUE) {
          if (juegoActivo) {
            int btnPressed = -1;
            if (evento.boton == BTN1) btnPressed = 0;
            else if (evento.boton == BTN2) btnPressed = 1;
            else if (evento.boton == BTN3) btnPressed = 2;

            if (btnPressed == ledActivo) {
              puntuacion++;
              Serial.println("¡Correcto!");
            } else {
              if (puntuacion > 0) puntuacion--;
              Serial.println("Incorrecto...");
            }
          }
          xSemaphoreGive(juegoMutex);
        }
        ultimoTiempo = evento.tiempo;
      }
    }
  }
}

void TareaTiempo(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(juegoMutex, portMAX_DELAY) == pdTRUE) {
      if (juegoActivo && tiempoJuego > 0) {
        tiempoJuego--;
        if (tiempoJuego == 0) {
          juegoActivo = false;
          desactivarTodosLEDs();
          digitalWrite(LED_STATUS, LOW);
          Serial.println("¡Tiempo agotado!");
        }
      }
      xSemaphoreGive(juegoMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void iniciarJuego() {
  if (xSemaphoreTake(juegoMutex, portMAX_DELAY) == pdTRUE) {
    tiempoJuego = 30;
    puntuacion = 0;
    juegoActivo = true;
    digitalWrite(LED_STATUS, HIGH);
    
    if (tareaJuegoHandle == NULL) {
      xTaskCreate(TareaJuego, "JuegoTask", 2048, NULL, 1, &tareaJuegoHandle);
    }
    xSemaphoreGive(juegoMutex);
  }
}

void detenerJuego() {
  if (xSemaphoreTake(juegoMutex, portMAX_DELAY) == pdTRUE) {
    juegoActivo = false;
    ledActivo = -1;
    desactivarTodosLEDs();
    digitalWrite(LED_STATUS, LOW);
    xSemaphoreGive(juegoMutex);
  }
}

void desactivarTodosLEDs() {
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
}

String obtenerEstadoJuego() {
  String estado = "{";
  if (xSemaphoreTake(juegoMutex, portMAX_DELAY) == pdTRUE) {
    estado += "\"score\":" + String(puntuacion) + ",";
    estado += "\"time\":" + String(tiempoJuego) + ",";
    estado += "\"led\":" + String(ledActivo) + ",";
    estado += "\"active\":" + String(juegoActivo ? "true" : "false") + ",";
    estado += "\"difficulty\":" + String(dificultad);
    xSemaphoreGive(juegoMutex);
  }
  estado += "}";
  return estado;
}

void enviarActualizacionWeb() {
  events.send(obtenerEstadoJuego().c_str(), "update", millis());
}