/*
This file is part of the Gamebuino-AKA library,
Copyright (c) Gamebuino 2026

This is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License (LGPL)
as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License (LGPL) for more details.

You should have received a copy of the GNU Lesser General Public
License (LGPL) along with the library.
If not, see <http://www.gnu.org/licenses/>.

Authors:
 - Jean-Marie Papillon
*/
#include "stdint.h"
#include "gb_ll_lcd.h"
#pragma once

class gb_graphics {
    public:
    gb_graphics();
     ~gb_graphics();
        //! clear screen with requested color
    void clear(uint16_t color);
        //! clear screen with current pen color (set by use setColor())
    void clear();
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
	void drawFastVLine(int16_t x, int16_t y, int16_t h);
	void drawFastHLine(int16_t x, int16_t y, int16_t w);
	void drawRect(int16_t x, int16_t y, int16_t w, int16_t h);
	void fillRect(int16_t x, int16_t y, int16_t w, int16_t h);
	void drawCircle(int16_t x0, int16_t y0, int16_t r);
	void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername);
	void fillCircle(int16_t x0, int16_t y0, int16_t r);
	void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta);
	void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
	void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
	void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius);
	void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius);
        // draw a simple pixel at corinates with current pen color (use setColor to change)
    inline void drawPixel( int16_t x, int16_t y, uint16_t u16_color ) {
        lcd_putpixel( x, y, u16_color );
    }
        // draw a simple pixel at corinates with requested color
    inline void drawPixel( int16_t x, int16_t y ) {
        lcd_putpixel( x, y, u16_color_pen );
    }
        // set current draw color for shapes and text
    inline void setColor( uint16_t u16_color ) {
        u16_color_pen = u16_color;
    }
        // make color from tree 8 bits RGB components (0..255)
    inline uint16_t makeColor(uint8_t red, uint8_t green, uint8_t blue) {
        return lcd_color_rgb(red, green, blue);
    }
        // draw ASCII char at location
    void draw_char( uint16_t x, uint16_t y, char c );
        // move text cursor to new pos
    void move_cursor( uint16_t x, uint16_t y );
        // draw a text string on screen
    void print_str(const char* string );
        // draw formated string to screen
    void printf(const char *pc_format, ...);
        //! set brigntness, 0=off, 255=max
    void set_backlight(uint16_t u16_duty);
        //! get brigntness, 0=off, 255=max
    uint16_t get_backlight();
        //! set as progressive percent 0..100
    void set_backlight_percent(uint8_t u8_percent);
        //! get as progressive percent 0..100
    uint8_t get_backlight_percent();
        //! set hard fps to @u8_fps, from 40 to 100
    void set_refresh_rate( uint8_t u8_fps );
	    //! return fps for last sec
    float get_fps();

        //! trasnfert screen buffer to screen
    void update();

    private:
        uint16_t u16_color_pen;
        uint16_t cursor_x = 0;
        uint16_t cursor_y = 0;
        uint16_t _u16_duty_lcd_pwm;
        uint8_t _u8_percent_lcd_pwm;
        uint32_t u32_last_stat_date = 0;
        uint32_t u32_last_stat_count = 0;
        float f32_fps_stat = 0;
};


