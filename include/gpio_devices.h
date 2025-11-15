#pragma once
#include <Arduino.h>
#include <stdlib.h>
#include <string>
#include <utility>

template <typename T>
class processor {
private:
  T (*processFunc)(int) = [] (int input) -> T {
    return input;
  };
public:
  processor(T (*processFunc)(int)) {
    this->processFunc = processFunc;
  }
  processor() {}
  T process(int input) {
    return this->processFunc(input);
  }
};

class gpio_device {
protected:
  int gpio_num;
  virtual void gpio_setup(int gpio_num) {}
public:
  gpio_device(int gpio_num) {
    this->gpio_num = gpio_num;
  }
};

template <typename T>
class gpio_sampler : public gpio_device, public processor<T> {
protected:
  int debug_value = 0;
  bool debug_mode = false;
  virtual void gpio_setup(int gpio_num) {
    pinMode(gpio_num, INPUT);
  }
public:
  gpio_sampler(int gpio_num) : gpio_device(gpio_num), processor<T>() {
    gpio_setup(gpio_num);
  }
  gpio_sampler(int gpio_num, T (*processFunc)(int)) : gpio_device(gpio_num), processor<T>(processFunc) {
    gpio_setup(gpio_num);
  }
  gpio_sampler(T (*processFunc)(int)) : gpio_device(-1), processor<T>(processFunc) {}
  virtual int read_raw() = 0;
  T read() {
    int raw_value = read_raw();
    return this->process(raw_value);
  }
  std::pair<int, T> read_debug() {
    int raw_value = read_raw();
    return std::make_pair(raw_value, this->process(raw_value));
  }
  void set_debug_value(int value) {
    if (value < 0)
      throw std::invalid_argument("Valor não pode ser negativo");
    if (value > 4095)
      throw std::invalid_argument("Valor não pode exceder 4095 (digital)");
    debug_value = value;
  }
  void set_debug_mode(bool mode) {
    debug_mode = mode;
  }
  bool get_debug_mode() const {
    return debug_mode;
  }
};

template <typename T>
class gpio_analog_sampler : public gpio_sampler<T> {
public:
  gpio_analog_sampler(int gpio_num) : gpio_sampler<T>(gpio_num) {}
  gpio_analog_sampler(int gpio_num, T (*processFunc)(int)) : gpio_sampler<T>(gpio_num, processFunc) {}
  gpio_analog_sampler(T (*processFunc)(int)) : gpio_sampler<T>(processFunc) {}
  int read_raw() override {
    if (this->debug_mode) {
      return this->debug_value;
    }
    return analogRead(this->gpio_num);
  }
};

template <typename T>
class gpio_digital_sampler : public gpio_sampler<T> {
public:
  gpio_digital_sampler(int gpio_num) : gpio_sampler<T>(gpio_num) {}
  gpio_digital_sampler(int gpio_num, T (*processFunc)(int)) : gpio_sampler<T>(gpio_num, processFunc) {}
  gpio_digital_sampler(T (*processFunc)(int)) : gpio_sampler<T>(processFunc) {}
  int read_raw() override {
    if (this->debug_mode) {
      return this->debug_value;
    }
    return digitalRead(this->gpio_num);
  }
};

class gpio_output : public gpio_device {
public:
  gpio_output() : gpio_device(-1) {}
  gpio_output(int gpio_num) : gpio_device(gpio_num) {
    pinMode(gpio_num, OUTPUT);
  }
  void write(uint32_t level) {
    digitalWrite(this->gpio_num, level);
  }
};
