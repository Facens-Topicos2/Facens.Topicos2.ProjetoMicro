void loop() { /* Não usado, toda a lógica está nas tasks */ }

#include <Arduino.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>
#include "gpio_devices.h"
#include "commands.h"

#define VOLTAGE_BASELINE 3.3   
#define TEMPERATURE_THRESHOLD 40.0

bool is_debug = false;
gpio_output led(2);
gpio_digital_sampler<int> button(0);
gpio_analog_sampler<double> battery_level_sensor(34);
gpio_analog_sampler<double> temperature_sensor(35);

#define DEBUG_TASK_PRIORITY         1
#define MONITOR_TEMP_TASK_PRIORITY  2
#define MONITOR_BATT_TASK_PRIORITY  3
#define ALARM_TASK_PRIORITY         4

#define DEBUG_TASK_FREQUENCY_MS        100
#define MONITOR_TEMP_TASK_FREQUENCY_MS 50
#define MONITOR_BATT_TASK_FREQUENCY_MS 50

TaskHandle_t alarmTaskHandle = NULL;

volatile bool alarm_active = false;

bool check_alarm_condition() {
  double batt = battery_level_sensor.read();
  double temp = temperature_sensor.read();
  return (batt <= VOLTAGE_BASELINE) || (temp >= TEMPERATURE_THRESHOLD);
}

void alarm_task(void* param) {
  gpio_output* output = static_cast<gpio_output*>(param);
  Serial.println("Task_Alarme pronta.");
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    Serial.println("Alarme disparado...");
    alarm_active = true;
    while (alarm_active) {
      output->write(1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      output->write(0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      if (!check_alarm_condition()) {
        alarm_active = false;
        Serial.println("Alarme finalizado.");
      }
    }
  }
}

void monitor_temp_task(void* param) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = MONITOR_TEMP_TASK_FREQUENCY_MS / portTICK_PERIOD_MS;
  for (;;) {
    double temp = temperature_sensor.read();
    if (temp >= TEMPERATURE_THRESHOLD && alarmTaskHandle != NULL && !alarm_active) {
      xTaskNotifyGive(alarmTaskHandle);
      Serial.println("Task_Temperatura liberou Task_Alarme.");
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void monitor_batt_task(void* param) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = MONITOR_BATT_TASK_FREQUENCY_MS / portTICK_PERIOD_MS;
  for (;;) {
    double batt = battery_level_sensor.read();
    if (batt <= VOLTAGE_BASELINE && alarmTaskHandle != NULL && !alarm_active) {
      xTaskNotifyGive(alarmTaskHandle);
      Serial.println("Task_Bateria liberou Task_Alarme.");
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void debug_task(void* param) {
  for (;;) {
    if (!Serial.available()) {
      vTaskDelay(DEBUG_TASK_FREQUENCY_MS / portTICK_PERIOD_MS);
      continue;
    }
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (!command.startsWith(":")) continue;
    try {
      std::string cmdKey = command.substring(0, command.indexOf(' ')).c_str();
      auto it = COMMANDS.find(cmdKey);
      if (it != COMMANDS.end()) {
        it->second(command);
      } else {
        Serial.println("Comando desconhecido");
      }
    } catch (const std::invalid_argument& e) {
      Serial.println(e.what());
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Sistema iniciado.");
  xTaskCreate(debug_task, "DebugTask", 4096, NULL, DEBUG_TASK_PRIORITY, NULL);
  xTaskCreate(monitor_temp_task, "MonitorTempTask", 2048, NULL, MONITOR_TEMP_TASK_PRIORITY, NULL);
  xTaskCreate(monitor_batt_task, "MonitorBattTask", 2048, NULL, MONITOR_BATT_TASK_PRIORITY, NULL);
  xTaskCreate(alarm_task, "AlarmTask", 2048, &led, ALARM_TASK_PRIORITY, &alarmTaskHandle);
}
