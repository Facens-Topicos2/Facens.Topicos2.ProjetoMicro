#include <Arduino.h>

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
  virtual void gpio_setup(gpio_num_t gpio_num);
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

  T read() {
    return this->process(gpio_get_level(this->gpio_num));
  }
};

class gpio_output : public gpio_device {
private:
  /* data */
public:
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

void setup() {
  gpio_output led(GPIO_NUM_2); // Create a GPIO output object for GPIO2
  gpio_sampler<int> button(GPIO_NUM_0, [] (int input) { return (1 - input) * 100; }); // Create a GPIO input object for GPIO0
  
  Serial.begin(9600);

  while (1) {
    int buttonState = button.read(); // Read the button state
    led.write(buttonState); // Set LED state based on button state
    Serial.printf("Button state: %d\n", buttonState);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
void loop() {}
