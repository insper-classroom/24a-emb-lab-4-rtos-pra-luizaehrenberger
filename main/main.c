#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "ssd1306.h"
#include "gfx.h"

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint ECHO_PIN = 16;
QueueHandle_t echoQueueDistance;

const uint TRIGGER_PIN = 3;
SemaphoreHandle_t triggerSemaphore;

ssd1306_t disp;

void pin_callback(uint gpio, uint32_t events){
    static BaseType_t xHigherPriorityTaskWoken;
    static absolute_time_t start_time;
    int64_t time_diff_us;

    if (events & GPIO_IRQ_EDGE_RISE) {
        // Início do sinal de eco
        start_time = get_absolute_time();
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        // Fim do sinal de eco
        absolute_time_t end_time = get_absolute_time();
        time_diff_us = absolute_time_diff_us(start_time, end_time);
        float distance_m = time_diff_us * 0.034 / 2;   
        // Enviar o tempo de eco para a fila xQueueTime
        xQueueSendFromISR(echoQueueDistance, &distance_m, &xHigherPriorityTaskWoken);
    }
}

void trigger_task(void *params){
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    while(true){
        if(xSemaphoreTake(triggerSemaphore, portMAX_DELAY)){
            // Enviar um pulso de 10us no pino TRIGGER_PIN
            gpio_put(TRIGGER_PIN, 1);
            sleep_us(10);
            gpio_put(TRIGGER_PIN, 0);

            // Aguardar 60ms antes de enviar um novo pulso
            vTaskDelay(pdMS_TO_TICKS(60));
        }
    }

}
void echo_task(void *params) {
    while(true){
        if(xSemaphoreTake(triggerSemaphore, portMAX_DELAY)){
            // Enviar um pulso de 10us no pino TRIGGER_PIN
            gpio_put(TRIGGER_PIN, 1);
            sleep_us(10);
            gpio_put(TRIGGER_PIN, 0);

            // Aguardar 60ms antes de enviar um novo pulso
            vTaskDelay(pdMS_TO_TICKS(60));
        }
    }
}

void oled_task(void *p) {
    float distance_cm;
    float scale = 0.64;
    //display OLED
    ssd1306_t disp;
    ssd1306_init();
    gfx_init(&disp, 128, 32);

    while (1) {
        if(xQueueReceive(echoQueueDistance, &distance_cm, portMAX_DELAY)){
            // A distância foi recebida, exibe no OLED

            // Limpa o buffer do display
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "Distancia:");
            // Prepara a string para mostrar a distância
            char buffer[16];
            snprintf(buffer, sizeof(buffer), "Dist: %.2f cm", distance_cm);
            gfx_draw_string(&disp, 0, 10, 1, buffer);
            int bar = (int)(distance_cm * scale* 100);
            if(bar > 128) {
                bar = 128;
            }
            gfx_draw_line(&disp, 0, 20, bar, 20);
            // Atualiza o display
            gfx_show(&disp);

            //libera semaforo trigger para enviar novo pulso
            xSemaphoreGive(triggerSemaphore);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main() {
    stdio_init_all();

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    ssd1306_init();
    gfx_init(&disp, 128, 32);

    triggerSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(triggerSemaphore);

    echoQueueDistance = xQueueCreate(5, sizeof(float));

    //xTaskCreate(oled1_demo_2, "Demo 2", 4095, NULL, 1, NULL);
    //xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger Task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 4095, NULL, 1, NULL);


    vTaskStartScheduler();
    return 0;

}