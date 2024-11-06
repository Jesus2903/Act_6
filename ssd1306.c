#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <pico/binary_info.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ssd1306.h"
#include "font.h"

inline static void swap(int32_t *a, int32_t *b) {
    int32_t *t=a;
    *a=*b;
    *b=*t;
}

inline static void fancy_write(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, char *name) {
    switch(i2c_write_blocking(i2c, addr, src, len, false)) {
    case PICO_ERROR_GENERIC:
        printf("[%s] addr not acknowledged!\n", name);
        break;
    case PICO_ERROR_TIMEOUT:
        printf("[%s] timeout!\n", name);
        break;
    default:
        break;
    }
}

inline static void ssd1306_write(ssd1306_t *p, uint8_t val) {
    uint8_t d[2]= {0x00, val};
    fancy_write(p->i2c_i, p->address, d, 2, "ssd1306_write");
}

bool ssd1306_init(ssd1306_t *p, uint16_t width, uint16_t height, uint8_t address, i2c_inst_t *i2c_instance) {
    p->width=width;
    p->height=height;
    p->pages=height/8;
    p->address=address;

    p->i2c_i=i2c_instance;


    p->bufsize=(p->pages)*(p->width);
    if((p->buffer=malloc(p->bufsize+1))==NULL) {
        p->bufsize=0;
        return false;
    }

    ++(p->buffer);

    uint8_t cmds[]= {
        SET_DISP,
        SET_DISP_CLK_DIV,
        0x80,
        SET_MUX_RATIO,
        height - 1,
        SET_DISP_OFFSET,
        0x00,
        SET_DISP_START_LINE,
        SET_CHARGE_PUMP,
        p->external_vcc?0x10:0x14,
        SET_SEG_REMAP | 0x01,           
        SET_COM_OUT_DIR | 0x08,         
        SET_COM_PIN_CFG,
        width>2*height?0x02:0x12,
        SET_CONTRAST,
        0xff,
        SET_PRECHARGE,
        p->external_vcc?0x22:0xF1,
        SET_VCOM_DESEL,
        0x30,                           
        SET_ENTIRE_ON,                  
        SET_NORM_INV,                   
        SET_DISP | 0x01,
        SET_MEM_ADDR,
        0x00,  
    };

    for(size_t i=0; i<sizeof(cmds); ++i)
        ssd1306_write(p, cmds[i]);

    return true;
}

inline void ssd1306_deinit(ssd1306_t *p) {
    free(p->buffer-1);
}

inline void ssd1306_poweroff(ssd1306_t *p) {
    ssd1306_write(p, SET_DISP|0x00);
}

inline void ssd1306_poweron(ssd1306_t *p) {
    ssd1306_write(p, SET_DISP|0x01);
}

inline void ssd1306_contrast(ssd1306_t *p, uint8_t val) {
    ssd1306_write(p, SET_CONTRAST);
    ssd1306_write(p, val);
}

inline void ssd1306_invert(ssd1306_t *p, uint8_t inv) {
    ssd1306_write(p, SET_NORM_INV | (inv & 1));
}

inline void ssd1306_clear(ssd1306_t *p) {
    memset(p->buffer, 0, p->bufsize);
}

void ssd1306_clear_pixel(ssd1306_t *p, uint32_t x, uint32_t y) {
    if(x>=p->width || y>=p->height) return;

    p->buffer[x+p->width*(y>>3)]&=~(0x1<<(y&0x07));
}

void ssd1306_draw_pixel(ssd1306_t *p, uint32_t x, uint32_t y) {
    if(x>=p->width || y>=p->height) return;

    p->buffer[x+p->width*(y>>3)]|=0x1<<(y&0x07); 
}

void ssd1306_draw_line(ssd1306_t *p, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if(x1>x2) {
        swap(&x1, &x2);
        swap(&y1, &y2);
    }

    if(x1==x2) {
        if(y1>y2)
            swap(&y1, &y2);
        for(int32_t i=y1; i<=y2; ++i)
            ssd1306_draw_pixel(p, x1, i);
        return;
    }

    float m=(float) (y2-y1) / (float) (x2-x1);

    for(int32_t i=x1; i<=x2; ++i) {
        float y=m*(float) (i-x1)+(float) y1;
        ssd1306_draw_pixel(p, i, (uint32_t) y);
    }
}

void ssd1306_clear_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    for(uint32_t i=0; i<width; ++i)
        for(uint32_t j=0; j<height; ++j)
            ssd1306_clear_pixel(p, x+i, y+j);
}

void ssd1306_draw_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    for(uint32_t i=0; i<width; ++i)
        for(uint32_t j=0; j<height; ++j)
            ssd1306_draw_pixel(p, x+i, y+j);
}

void ssd1306_draw_empty_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    ssd1306_draw_line(p, x, y, x+width, y);
    ssd1306_draw_line(p, x, y+height, x+width, y+height);
    ssd1306_draw_line(p, x, y, x, y+height);
    ssd1306_draw_line(p, x+width, y, x+width, y+height);
}

void ssd1306_draw_char_with_font(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t *font, char c) {
    if(c<font[3]||c>font[4])
        return;

    uint32_t parts_per_line=(font[0]>>3)+((font[0]&7)>0);
    for(uint8_t w=0; w<font[1]; ++w) { // width
        uint32_t pp=(c-font[3])*font[1]*parts_per_line+w*parts_per_line+5;
        for(uint32_t lp=0; lp<parts_per_line; ++lp) {
            uint8_t line=font[pp];

            for(int8_t j=0; j<8; ++j, line>>=1) {
                if(line & 1)
                    ssd1306_draw_square(p, x+w*scale, y+((lp<<3)+j)*scale, scale, scale);
            }

            ++pp;
        }
    }
}

void ssd1306_draw_string_with_font(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t *font, const char *s) {
    for(int32_t x_n=x; *s; x_n+=(font[1]+font[2])*scale) {
        ssd1306_draw_char_with_font(p, x_n, y, scale, font, *(s++));
    }
}

void ssd1306_draw_char(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, char c) {
    ssd1306_draw_char_with_font(p, x, y, scale, font_8x5, c);
}

void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s) {
    ssd1306_draw_string_with_font(p, x, y, scale, font_8x5, s);
}

static inline uint32_t ssd1306_bmp_get_val(const uint8_t *data, const size_t offset, uint8_t size) {
    switch(size) {
    case 1:
        return data[offset];
    case 2:
        return data[offset]|(data[offset+1]<<8);
    case 4:
        return data[offset]|(data[offset+1]<<8)|(data[offset+2]<<16)|(data[offset+3]<<24);
    default:
        __builtin_unreachable();
    }
    __builtin_unreachable();
}


