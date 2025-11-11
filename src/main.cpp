#include <Arduino.h>
#include <stdlib.h>
#include <vector>
#include <string>

#pragma region routines
template<typename T>
class task {
private:
  TaskHandle_t taskHandle = NULL;
  std::function<T()> func;
  const char* name;
  int stackSize;
  int priority;
  bool started = false;
public:
  task(std::function<T()> func, const char* name = "task", int stackSize = 4096, int priority = 1)
    : func(func), name(name), stackSize(stackSize), priority(priority) {}

  void start() {
    if (!started) {
      auto* funcPtr = new std::function<T()>(func);
      xTaskCreate([](void* param) {
        auto* func = static_cast<std::function<T()>*>(param);
        func->operator()();
        delete func;
        vTaskDelete(NULL);
      }, name, stackSize, funcPtr, priority, &taskHandle);
      started = true;
    }
  }

  void stop() {
    if (taskHandle != NULL) {
      vTaskDelete(taskHandle);
      taskHandle = NULL;
      started = false;
    }
  }
};

class task_routine {
private:
  task<void> routineTask;
public:
  task_routine(std::function<void()> routineFunc, const char* name = "routineTask", int stackSize = 4096, int priority = 1)
    : routineTask(routineFunc, name, stackSize, priority) {}

  void start() {
    routineTask.start();
  }

  void stop() {
    routineTask.stop();
  }
};
#pragma endregion

#pragma region processor
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
#pragma endregion

#pragma region gpio
class gpio_device
{
protected:
  gpio_num_t gpio_num;
  virtual void gpio_setup(gpio_config_t io_conf) {
    gpio_config(&io_conf);
  }
  virtual void gpio_setup(gpio_num_t gpio_num) {}
public:
  gpio_device(gpio_num_t gpio_num) {
    this->gpio_num = gpio_num;
  }
};

template <typename T>
class gpio_sampler : public gpio_device, public processor<T> {
protected:
  virtual void gpio_setup(gpio_num_t gpio_num) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
  }
public:
  gpio_sampler(gpio_num_t gpio_num) : gpio_device(gpio_num), processor<T>() {
    gpio_setup(gpio_num);
  }

  gpio_sampler(gpio_num_t gpio_num, T (*processFunc)(int)) : gpio_device(gpio_num), processor<T>(processFunc) {
    gpio_setup(gpio_num);
  }

  gpio_sampler(T (*processFunc)(int)) : gpio_device(GPIO_NUM_NC), processor<T>(processFunc) {}

  T read() {
    T result;
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    auto taskFunc = [this, &result, caller]() -> T {
      result = this->process(gpio_get_level(this->gpio_num));
      xTaskNotifyGive(caller);
      return result;
    };
    task<T> oneShotTask(taskFunc, "gpioSamplerOneShot");
    oneShotTask.start();
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return result;
  }

  void read_async(TaskHandle_t notifyTask) {
    auto taskFunc = [this, notifyTask]() -> T {
      T result = this->process(gpio_get_level(this->gpio_num));
      xTaskNotifyGive(notifyTask);
      return result;
    };
    task<T>* oneShotTask = new task<T>(taskFunc, "gpioSamplerOneShotAsync");
    oneShotTask->start();
  }

  void read_async_callback(std::function<void(T)> callback) {
    auto taskFunc = [this, callback]() -> T {
      T result = this->process(gpio_get_level(this->gpio_num));
      callback(result);
      return result;
    };
    task<T>* oneShotTask = new task<T>(taskFunc, "gpioSamplerOneShotCallback");
    oneShotTask->start();
  }
};

class gpio_output : public gpio_device {
private:
  /* data */
public:
  gpio_output() : gpio_device(GPIO_NUM_NC) {}

  gpio_output(gpio_num_t gpio_num) : gpio_device(gpio_num) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,    
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE    
    };
    gpio_config(&io_conf);
  }

  void write(uint32_t level) {
    gpio_set_level(this->gpio_num, level);
  }
};
#pragma endregion

gpio_output led(GPIO_NUM_2);
gpio_sampler<int> button(GPIO_NUM_0, [] (int input) { return 1 - input; });

gpio_sampler<int> battery_level_sensor([] (int input) {
  // Simula leitura do nível de bateria
  return 0; 
});

gpio_sampler<int> temperature_sensor([] (int input) {
  // Simula leitura da temperatura
  return 0; 
});

std::function<void(gpio_output*)> alarmTaskFunc = [](gpio_output* output) {
  Serial.println("Alarme disparado...");
  for(;;) {
    output->write(1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    output->write(0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
};
task_routine alarm_task = task_routine([&]() { alarmTaskFunc(&led); });

void setup() {
  Serial.begin(9600);
}

void loop() {
  if (button.read()) {
    alarm_task.start();
  }
}
