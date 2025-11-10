#include <Arduino.h>

void setup() {
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << GPIO_NUM_2), // Configure GPIO2
      .mode = GPIO_MODE_OUTPUT,             // Set as output mode
      .pull_up_en = GPIO_PULLUP_DISABLE,    // Disable pull-up
      .pull_down_en = GPIO_PULLDOWN_DISABLE,// Disable pull-down
      .intr_type = GPIO_INTR_DISABLE        // Disable interrupt
  };

  gpio_config(&io_conf); // Apply the configuration

  while (1) {
    // Set GPIO2 high
    gpio_set_level(GPIO_NUM_2, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS); // Delay for 0.5 second

    // Set GPIO2 low
    gpio_set_level(GPIO_NUM_2, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS); // Delay for 0.5 second
  }
}

void loop() {}