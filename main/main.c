/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint ECHO_PIN;
QueueHandle_t echoQueueTime;
QueueHandle_t echoQueueDistance;

const uint TRIGGER_PIN;
SemaphoreHandle_t triggerSemaphore;

void pin_callback(uint gpio, uint32_t events){
    static absolute_time_t start_time;
    absolute_time_t end_time;
    int64_t time_diff_us;

    if (events & GPIO_IRQ_EDGE_RISE) {
        // Início do sinal de eco
        start_time = get_absolute_time();
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        // Fim do sinal de eco
        end_time = get_absolute_time();
        time_diff_us = absolute_time_diff_us(start_time, end_time);

        // Enviar o tempo de eco para a fila xQueueTime
        xQueueSendFromISR(echoQueueTime, &time_diff_us, NULL);
    }
}

void trigger_task(){
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
    int64_t start_time_us = 0, end_time_us = 0, duration_us = 0;
    float distance_cm = 0;
    int64_t time_diff_us = 0;

    while(true){
        if(xQueueReceive(echoQueueTime, &time_diff_us, portMAX_DELAY)){
            // inicio ou fim do pulso
            if(start_time_us == 0) {
                start_time_us = time_diff_us;
            } else {
                end_time_us = time_diff_us;
                duration_us = end_time_us - start_time_us;
                distance_cm = duration_us * 0.034 / 2;
                start_time_us = 0;
                
                xQueueSend(echoQueueDistance, &distance_cm, portMAX_DELAY);
            }

            // Enviar a distância para a fila xQueueDistance
            xQueueSend(echoQueueDistance, &distance_cm, portMAX_DELAY);
        }
    
    }
}

void oled_task(void *p) {
    float distance_cm = 0;

    //display OLED
    ssd1306_t disp;
    ssd1306_init();
    gfx_init(&disp, 128, 32);

    while (1) {
        if(xQueueReceive(echoQueueDistance, &distance_cm, portMAX_DELAY)){
            // A distância foi recebida, exibe no OLED

            // Limpa o buffer do display
            gfx_clear_buffer(&disp);

            // Prepara a string para mostrar a distância
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "Dist: %.2f cm", distance_cm);
            gfx_draw_string(&disp, 0, 0, 1, buffer);

            // Atualiza o display
            gfx_show(&disp);

            //libera semaforo trigger para enviar novo pulso
            xSemaphoreGive(triggerSemaphore);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void oled1_demo_1(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char cnt = 15;
    while (1) {

        if (gpio_get(BTN_1_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_1_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 1 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_2_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_2_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 2 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_3_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_3_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 3 - ON");
            gfx_show(&disp);
        } else {

            gpio_put(LED_1_OLED, 1);
            gpio_put(LED_2_OLED, 1);
            gpio_put(LED_3_OLED, 1);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "PRESSIONE ALGUM");
            gfx_draw_string(&disp, 0, 10, 1, "BOTAO");
            gfx_draw_line(&disp, 15, 27, cnt,
                          27);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (++cnt == 112)
                cnt = 15;

            gfx_show(&disp);
        }
    }
}

void oled1_demo_2(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    while (1) {

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, "Mandioca");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 2, "Batata");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 4, "Inhame");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

int main() {
    stdio_init_all();

    echoQueueTime = xQueueCreate(5, sizeof(int64_t));
    triggerSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(triggerSemaphore);

    echoQueueDistance = xQueueCreate(5, sizeof(float));
    if(echoQueueTime == NULL || echoQueueDistance == NULL){
        printf("Erro ao criar fila\n");
        return 1;
    }

    xTaskCreate(oled1_demo_2, "Demo 2", 4095, NULL, 1, NULL);
    xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4095, NULL, 1, NULL);


    vTaskStartScheduler();
    return 0;

}
