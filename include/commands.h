#pragma once
#include <map>
#include <string>
#include <functional>
#include <Arduino.h>
#include "gpio_devices.h"

extern bool is_debug;
extern gpio_output led;
extern gpio_digital_sampler<int> button;
extern gpio_analog_sampler<double> battery_level_sensor;
extern gpio_analog_sampler<double> temperature_sensor;

static const std::map<std::string, std::function<void(String)>> COMMANDS = {
  {":set_volt", [](String command) {
    int value = command.substring(9).toInt();
    battery_level_sensor.set_debug_value(value);
    String output = "Nivel de bateria definido";
    Serial.println(output);
  }},
  {":set_temp", [](String command) {
    int value = command.substring(9).toInt();
    temperature_sensor.set_debug_value(value);
    String output = "Temperatura definida";
    Serial.println(output);
  }},
  {":set_debug", [](String command) {
      bool value = command.substring(10).toInt();
      battery_level_sensor.set_debug_mode(value);
      temperature_sensor.set_debug_mode(value);
      is_debug = value;
      if (!value) {
        Serial.println("Modo debug desativado");
      } else {
        Serial.println("Modo debug ativado");
      }
  }},
  {":read", [](String command) {
    auto [battery_level_raw, battery_level] = battery_level_sensor.read_debug();
    auto [temperature_raw, temperature] = temperature_sensor.read_debug();
    bool debug_mode = battery_level_sensor.get_debug_mode();
    String output =
      "Debug: " + String(debug_mode ? "Ativo" : "Inativo") + "\n" +
      "Nivel de bateria: " + String(battery_level) + " V | " + String(battery_level_raw) + " (digital)\n" +
      "Temperatura: " + String(temperature) + " C | " + String(temperature_raw) + " (digital)";
    Serial.print(output);
  }},
  {":help", [](String command) {
      String output =
        "Comandos disponiveis:\n\
        \t:set_debug <valor> - Ativa o modo debug\n\
        \t:set_volt <valor> - Define o nivel de bateria (modo debug)\n\
        \t:set_temp <valor> - Define a temperatura (modo debug)\n\
        \t:read - Leitura dos sensores\n\
        \t:help - Mostra esta ajuda";
      Serial.println(output);
  }},
};
