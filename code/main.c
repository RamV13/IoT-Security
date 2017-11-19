/*
 * File:        TFT, keypad, DAC, LED, PORT EXPANDER test
 *              With serial interface to PuTTY console
 * Author:      Bruce Land
 * For use with Sean Carroll's Big Board
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config.h"
// threading library
#include "pt_cornell_1_2_2.h"

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
// need for sin function
#include <math.h>
////////////////////////////////////

// lock out timer 2 interrupt during spi comm to port expander
// This is necessary if you use the SPI2 channel in an ISR.
#define start_spi2_critical_section INTEnable(INT_T2, 0)
#define end_spi2_critical_section INTEnable(INT_T2, 1)


////////////////////////////////////

/* Demo code for interfacing TFT (ILI9340 controller) to PIC32
 * The library has been modified from a similar Adafruit library
 */
// Adafruit data:
/***************************************************
  This is an example sketch for the Adafruit 2.2" SPI display.
  This library works with the Adafruit 2.2" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/1480

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

// string buffer
char buffer[60];

////////////////////////////////////
// DAC ISR
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000
// DDS constant
#define two32 4294967296.0 // 2^32 
#define Fs 100000

//== Timer 2 interrupt handler ===========================================
volatile unsigned int DAC_data ;// output value
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 4 ; // 10 MHz max speed for port expander!!
// the DDS units:
volatile unsigned int phase_accum_main, phase_incr_main=400.0*two32/Fs ;//
// DDS sine table
#define sine_table_size 256
volatile int sin_table[sine_table_size];

// === print a line on TFT =====================================================
// print a line on the TFT
// string buffer
char buffer[60];
void printLine(int line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 10 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 239, 8, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(1);
    tft_writeString(print_buffer);
}

void printLine2(int line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 20 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 239, 16, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(2);
    tft_writeString(print_buffer);
}

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_serial ;
// The following threads are necessary for UART control
static struct pt pt_input, pt_output, pt_DMA_output ;

// system 1 second interval tick
int sys_time_seconds ;

#define uart_send(msg) { \
  sprintf(PT_send_buffer, msg "\r\n"); \
  PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output)); \
}

#define uart_send_raw(msg) { \
  sprintf(PT_send_buffer, msg); \
  PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output)); \
}

#define uart_recv(res) { \
  PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input)); \
  sprintf(res, PT_term_buffer); \
}

#define uart_recv_char(res) { \
  PT_SPAWN(pt, &pt_input, PT_GetSerialBufferChar(&pt_input)); \
  sprintf(res, PT_term_buffer); \
}

#define TIMEOUT  10

#define DEBUG  true

#define wait_recv(res, msg) { \
  res[0] = 0; \
  int count = 0; \
  int num_lines = 0; \
  while (strcmp(res, msg) != 0) { \
    uart_recv(res); \
    if (DEBUG) { \
      tft_setCursor(0, 10 + num_lines * 10); \
      tft_writeString(">"); \
      tft_setCursor(10, 10 + num_lines * 10); \
      tft_writeString(res); \
      num_lines++; \
    } \
    count++; \
    if (count == TIMEOUT) { \
      tft_fillScreen(ILI9340_RED); \
      break; \
    } \
  } \
}

#define wait_recv_char(res, msg) { \
  res[0] = 0; \
  int count = 0; \
  int num_lines = 0; \
  while (strcmp(res, msg) != 0) { \
    uart_recv_char(res); \
    if (DEBUG) { \
      tft_setCursor(0, 10 + num_lines * 10); \
      tft_writeString(">"); \
      tft_setCursor(10, 10 + num_lines * 10); \
      tft_writeString(res); \
      num_lines++; \
    } \
    count++; \
    if (count == TIMEOUT) { \
      tft_fillScreen(ILI9340_RED); \
      break; \
    } \
  } \
}

//=== Serial terminal thread =================================================
static PT_THREAD (protothread_serial(struct pt *pt)) {
  PT_BEGIN(pt);

  char res[32];

  tft_setTextSize(1);
  tft_setTextColor(ILI9340_WHITE);

  uart_send("ATE0"); // disable echo
  wait_recv(res, "OK");
  PT_YIELD_TIME_msec(1000);
  tft_fillScreen(ILI9340_BLACK);
  uart_send("AT");
  wait_recv(res, "OK");
  PT_YIELD_TIME_msec(1000);
  tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPMUX=0");
  wait_recv(res, "OK");
  PT_YIELD_TIME_msec(1000);
  tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPSTART=\"TCP\",\"104.131.124.11\",4000");
  wait_recv(res, "OK");
  PT_YIELD_TIME_msec(1000);
  tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPSEND=2");
  wait_recv_char(res, ">");
  PT_YIELD_TIME_msec(1000);
  tft_fillScreen(ILI9340_BLACK);
  uart_send_raw("s");
  wait_recv(res, "SEND OK");
  tft_fillScreen(ILI9340_BLACK);

  int num_lines = 0;
  while (1) {
    uart_recv(res);
    tft_setCursor(0, 10 + num_lines * 10);
    tft_writeString(">");
    tft_setCursor(10, 10 + num_lines * 10);
    if (strcmp(res, "\n") == 0) {
      tft_writeString("\\n");
    } else if (strcmp(res, "\r") == 0) {
      tft_writeString("\\r");
    } else {
      tft_writeString(res);
    }
    num_lines++;
  }

/*
  int count = 0;
  while (1) {
    tft_fillScreen(ILI9340_BLUE);
    uart_recv(res);
    const char* ptr = strchr(res, ':');
    char length_string[8];
    char data[32];
    if (ptr) {
      tft_fillScreen(ILI9340_RED);
      int index = ptr - res;
      memcpy(length_string, &res[7], index - 7);
      int length = atoi(length_string);
      memcpy(data, &res[9], length);
      tft_setCursor(10, 100);
      tft_setTextColor(ILI9340_WHITE);
      tft_setTextSize(2);
      tft_writeString(data);
    }
    count++;
    if (count == TIMEOUT) {
      tft_setCursor(10, 100);
      tft_setTextColor(ILI9340_WHITE);
      tft_setTextSize(2);
      tft_writeString("FUCK");
      break;
    }
    PT_YIELD_TIME_msec(1000);
  }

  PT_YIELD_TIME_msec(1000);
  tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPCLOSE=0");
  wait_recv(res, "OK");
*/

  while(1) {
    PT_YIELD_TIME_msec(1000);
  } // END WHILE(1)
  PT_END(pt);
} // thread 3

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK);
  
  ANSELA = 0; ANSELB = 0; 

  // === config threads ==========
  // turns OFF UART support and debugger pin, unless defines are set
  PT_setup();

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // init the threads
  PT_INIT(&pt_serial);

  // init the display
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240
  
  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_serial(&pt_serial));
  }
} // main

// === end  ======================================================

