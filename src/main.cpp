void loop() { /* Não usado, toda a lógica está nas tasks */ }

#include <Arduino.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>
#include "gpio_devices.h"
#include "commands.h"


#define DEBUG_TASK_PRIORITY         1
#define MONITOR_TEMP_TASK_PRIORITY  2
#define MONITOR_BATT_TASK_PRIORITY  3
#define ALARM_TASK_PRIORITY         4

#define DEBUG_TASK_FREQUENCY_MS        1000
#define MONITOR_TEMP_TASK_FREQUENCY_MS 50
#define MONITOR_BATT_TASK_FREQUENCY_MS 50

#define VOLTAGE_REFERENCE 3.3
#define ADC_MAX_VALUE 4095

#define TEMPERATURE_BASELINE 20.0
#define TEMPERATURE_MAX 100.0
#define TEMPERATURE_ALARM_THRESHOLD_UPPER 80.0
#define TEMPERATURE_RESOLUTION ((TEMPERATURE_MAX - TEMPERATURE_BASELINE) / ADC_MAX_VALUE)

#define BATTERY_BASELINE 0.0
#define BATTERY_MAX 100.0
#define BATTERY_VOLTAGE_ALARM_THRESHOLD_LOWER 20.0
#define BATTERY_RESOLUTION ((BATTERY_MAX - BATTERY_BASELINE) / ADC_MAX_VALUE)

bool is_debug = false;
bool debug_battery = false;
bool debug_temperature = false;
bool debug_logs = false;

int alarm_task_called_ticks = 0;

// Task execution time queues (stores last 10 execution times)
std::vector<unsigned long> monitor_temp_times;
std::vector<unsigned long> monitor_batt_times;
std::vector<unsigned long> alarm_times;
std::vector<unsigned long> debug_times;
const size_t MAX_TIMING_SAMPLES = 10;

gpio_output led(2);

gpio_analog_sampler<double> battery_level_sensor(34, [] (int input) -> double {
    return input * BATTERY_RESOLUTION + BATTERY_BASELINE;
}, &debug_battery);

gpio_analog_sampler<double> temperature_sensor(35, [] (int input) -> double {
    return input * TEMPERATURE_RESOLUTION + TEMPERATURE_BASELINE;
}, &debug_temperature);
TaskHandle_t alarmTaskHandle = NULL;

volatile bool alarm_active = false;

bool check_alarm_condition() {
  double batt = battery_level_sensor.read();
  double temp = temperature_sensor.read();
  return (batt <= BATTERY_VOLTAGE_ALARM_THRESHOLD_LOWER) || (temp >= TEMPERATURE_ALARM_THRESHOLD_UPPER);
}

void alarm_task(void* param) {
  gpio_output* output = static_cast<gpio_output*>(param);
  Serial.println("\nTask_Alarme pronta.");
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    int alarm_task_start_delay = (xTaskGetTickCount() - alarm_task_called_ticks) * portTICK_PERIOD_MS; 
    Serial.println("\nAlarme disparado... (atraso na chamada da task: " + String(alarm_task_start_delay) + " ms)");
    alarm_active = true;
    while (alarm_active) {
      unsigned long start = millis();
      output->write(1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      output->write(0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      if (!check_alarm_condition()) {
        alarm_active = false;
        Serial.println("\nAlarme finalizado.");
      }
      if (debug_logs) {
        unsigned long elapsed = millis() - start;
        alarm_times.push_back(elapsed);
        if (alarm_times.size() > MAX_TIMING_SAMPLES) {
          alarm_times.erase(alarm_times.begin());
        }
      }
    }
  }
}

void monitor_temp_task(void* param) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = MONITOR_TEMP_TASK_FREQUENCY_MS / portTICK_PERIOD_MS;
  for (;;) {
    unsigned long start = millis();
    double temp = temperature_sensor.read();
    if (temp >= TEMPERATURE_ALARM_THRESHOLD_UPPER && alarmTaskHandle != NULL && !alarm_active) {
      alarm_task_called_ticks = xTaskGetTickCount();
      xTaskNotifyGive(alarmTaskHandle);
      Serial.println("\nTask_Temperatura liberou Task_Alarme.");
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    if (debug_logs) {
      unsigned long elapsed = millis() - start;
      monitor_temp_times.push_back(elapsed);
      if (monitor_temp_times.size() > MAX_TIMING_SAMPLES) {
        monitor_temp_times.erase(monitor_temp_times.begin());
      }
    }
  }
}

void monitor_batt_task(void* param) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = MONITOR_BATT_TASK_FREQUENCY_MS / portTICK_PERIOD_MS;
  for (;;) {
    unsigned long start = millis();
    double batt = battery_level_sensor.read();
    if (batt <= BATTERY_VOLTAGE_ALARM_THRESHOLD_LOWER && alarmTaskHandle != NULL && !alarm_active) {
      alarm_task_called_ticks = xTaskGetTickCount();
      xTaskNotifyGive(alarmTaskHandle);
      Serial.println("\nTask_Bateria liberou Task_Alarme.");
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    if (debug_logs) {
      unsigned long elapsed = millis() - start;
      monitor_batt_times.push_back(elapsed);
      if (monitor_batt_times.size() > MAX_TIMING_SAMPLES) {
        monitor_batt_times.erase(monitor_batt_times.begin());
      }
    }
  }
}

void debug_task(void* param) {
  for (;;) {
    unsigned long task_start = millis();
    unsigned long start = millis();
    String command = "";
    unsigned long timeout = DEBUG_TASK_FREQUENCY_MS;
    unsigned long start_time = millis();
    while ((millis() - start_time) < timeout && command.isEmpty()) {
      while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') break;
        command += c;
      }
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (command.isEmpty()) {
      if (debug_logs) {
        auto readIt = COMMANDS.find(":read");
        if (readIt != COMMANDS.end()) {
          readIt->second(":read");
        }
      }
    } else {
      command.trim();
      if (command.startsWith(":")) {
        std::string cmdKey = command.substring(0, command.indexOf(' ')).c_str();
        try {
          auto it = COMMANDS.find(cmdKey);
          if (it != COMMANDS.end()) {
            it->second(command);
          } else {
            Serial.println("\nComando desconhecido");
          }
        } catch (const std::invalid_argument& e) {
          Serial.println(e.what());
        }

        if (debug_logs && cmdKey != ":read") {
          auto readIt = COMMANDS.find(":read");
          if (readIt != COMMANDS.end()) {
            Serial.println("\n");
            readIt->second(":read");
            Serial.println("\n");
          }
        }    
      }
    }
    
    unsigned long total_elapsed = millis() - task_start;
    if (total_elapsed < DEBUG_TASK_FREQUENCY_MS) {
      vTaskDelay((DEBUG_TASK_FREQUENCY_MS - total_elapsed) / portTICK_PERIOD_MS);
    }

    if (debug_logs) {
      unsigned long elapsed = millis() - start;
      debug_times.push_back(elapsed);
      if (debug_times.size() > MAX_TIMING_SAMPLES) {
        debug_times.erase(debug_times.begin());
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nSistema iniciado.");
  Serial.println("\nPara ver os comandos disponiveis, digite :help");
  xTaskCreate(debug_task, "DebugTask", 4096, NULL, DEBUG_TASK_PRIORITY, NULL);
  xTaskCreate(monitor_temp_task, "MonitorTempTask", 2048, NULL, MONITOR_TEMP_TASK_PRIORITY, NULL);
  xTaskCreate(monitor_batt_task, "MonitorBattTask", 2048, NULL, MONITOR_BATT_TASK_PRIORITY, NULL);
  xTaskCreate(alarm_task, "AlarmTask", 2048, &led, ALARM_TASK_PRIORITY, &alarmTaskHandle);
}
