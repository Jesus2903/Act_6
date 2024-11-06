#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"


const uint8_t num_chars_per_disp[]={7,7,7,5};


#define SLEEPTIME 100

void inicializar(void);
void imprimir(void);

int main() {
    stdio_init_all();

    inicializar();
    imprimir();

    return 0;
}


void inicializar(void) {
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);
}


void imprimir(void) {
    const char *words[]= {"Jesus", "Morales", "Vazquez"};

    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);
    char buf[7];

    for(;;) {
        
        for(int i=0; i<sizeof(words)/sizeof(char *); ++i) {
            ssd1306_draw_string(&disp, 8, 24, 2, words[i]);
            ssd1306_show(&disp);
            sleep_ms(800);
            ssd1306_clear(&disp);
        }        
    }
}
