#include <Arduino.h>
#include <freertos/task.h>   // Necesario para xTaskCreate

// Declaración anticipada (o mueve la función arriba)
void anotherTask(void * parameter);

void setup()
{
  Serial.begin(112500);
  xTaskCreate(
    anotherTask,      /* Task function */
    "another Task",   /* name of task */
    10000,            /* Stack size */
    NULL,             /* parameter */
    1,                /* priority */
    NULL);            /* handle */
}

void loop()
{
  Serial.println("this is ESP32 Task");
  delay(1000);
}

void anotherTask(void * parameter)
{
  for(;;)
  {
    Serial.println("this is another Task");
    delay(1000);
  }
  vTaskDelete(NULL);   // nunca se ejecuta
}