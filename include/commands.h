#pragma once
#include <map>
#include <string>
#include <functional>
#include <Arduino.h>
#include "gpio_devices.h"

extern bool is_debug;
extern bool debug_battery;
extern bool debug_temperature;
extern bool debug_logs;
extern std::vector<unsigned long> monitor_temp_times;
extern std::vector<unsigned long> monitor_batt_times;
extern std::vector<unsigned long> alarm_times;
extern std::vector<unsigned long> debug_times;
extern gpio_output led;
extern gpio_digital_sampler<int> button;
extern gpio_analog_sampler<double> battery_level_sensor;
extern gpio_analog_sampler<double> temperature_sensor;

static const std::map<std::string, std::function<void(String)>> COMMANDS = {
  {":set_battery", [](String command) {
    int value = command.substring(12).toInt();
    battery_level_sensor.set_debug_value(value);
    String output = "\nNivel de bateria definido";
    Serial.println(output);
  }},
  {":set_temp", [](String command) {
    int value = command.substring(9).toInt();
    temperature_sensor.set_debug_value(value);
    String output = "\nTemperatura definida";
    Serial.println(output);
  }},
    {":set_debug", [](String command) {
        String arg = command.substring(10);
        arg.trim();
        if (arg.length() == 1) {
          // :set_debug 1 or :set_debug 0
          bool value = arg.toInt();
          debug_battery = value;
          debug_temperature = value;
          is_debug = value;
          Serial.println(value ? "\nModo debug ativado para todos" : "\nModo debug desativado para todos");
        } else if (arg.length() >= 3) {
          char flag = arg.charAt(0);
          bool value = arg.substring(2).toInt();
          switch (flag) {
            case 'b':
              debug_battery = value;
              battery_level_sensor.set_debug_mode(value);
              Serial.println(value ? "\nModo debug ativado para bateria" : "\nModo debug desativado para bateria");
              break;
            case 't':
              debug_temperature = value;
              temperature_sensor.set_debug_mode(value);
              Serial.println(value ? "\nModo debug ativado para temperatura" : "\nModo debug desativado para temperatura");
              break;
            case 'l':
              debug_logs = value;
              Serial.println(value ? "\nModo debug ativado para logs" : "\nModo debug desativado para logs");
              break;
            default:
              Serial.println("\nFlag desconhecida. Use b, t ou l.");
          }
        } else {
          Serial.println("\nUso: :set_debug [b|t|l] <0|1>");
        }
    }},
  {":read", [](String command) {
    auto [battery_level_raw, battery_level] = battery_level_sensor.read_debug();
    auto [temperature_raw, temperature] = temperature_sensor.read_debug();
    std::vector<String> active_flags;
    if (debug_battery) active_flags.push_back("B");
    if (debug_temperature) active_flags.push_back("T");
    if (debug_logs) active_flags.push_back("L");
    String debug_output;
    if (active_flags.size() == 3) {
        debug_output = "Debug: Ativo";
    } else if (!active_flags.empty()) {
        String joined_flags = "";
        for (size_t i = 0; i < active_flags.size(); ++i) {
            joined_flags += active_flags[i];
            if (i < active_flags.size() - 1) joined_flags += " | ";
        }
        debug_output = "Debug: " + joined_flags;
    } else {
        debug_output = "Debug: Inativo";
    }
    
    auto calc_avg = [](const std::vector<unsigned long>& times) -> float {
      if (times.empty()) return 0.0f;
      unsigned long sum = 0;
      for (auto t : times) sum += t;
      return (float)sum / times.size();
    };
    
    String timing_output = "";
    if (!monitor_temp_times.empty() || !monitor_batt_times.empty() || 
        !alarm_times.empty() || !debug_times.empty()) {
      timing_output = "\n\nTempo medio de execucao (ultimas 10 amostras):\n";
      timing_output += "\tMonitorTempTask: " + String(calc_avg(monitor_temp_times), 2) + " ms\n";
      timing_output += "\tMonitorBattTask: " + String(calc_avg(monitor_batt_times), 2) + " ms\n";
      timing_output += "\tAlarmTask: " + String(calc_avg(alarm_times), 2) + " ms\n";
      timing_output += "\tDebugTask: " + String(calc_avg(debug_times), 2) + " ms";
    }
    
    String output =
      debug_output + "\n" +
      "\tNivel de bateria: " + String(battery_level) + " % | " + String(battery_level_raw) + " (digital)\n" +
      "\tTemperatura: " + String(temperature) + " C | " + String(temperature_raw) + " (digital)" +
      timing_output;
    Serial.print(output);
  }},
  {":help", [](String command) {
      String output =
        "\nComandos disponiveis:\n\
  :set_debug <0|1> - Ativa/desativa todos os modos de debug\n\
  :set_debug [b|t|l] <0|1> - Ativa/desativa debug específico: b (bateria), t (temperatura), l (logs)\n\
  :set_battery <valor> - Define o nível de bateria (modo debug)\n\
  :set_temp <valor> - Define a temperatura (modo debug)\n\
  :read - Leitura dos sensores e exibe os flags de debug ativos (B, T, L)\n\
  :help - Mostra esta ajuda";
      Serial.println(output);
  }},
};
