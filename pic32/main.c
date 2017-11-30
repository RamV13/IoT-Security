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

// graphics libraries
#include "tft_master.h"
#include "tft_gfx.h"
#include "utils/tft/tft_master.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

////////////////////////////////////
// DAC ISR
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1xDAC_config, active
#define DAC_config_chan_B 0b1011000000000000
// DDS constant
// #define two32 4294967296.0 // 2^32 
// #define Fs 100000

//== Timer 2 interrupt handler ===========================================
volatile unsigned int DAC_data; // output value
// volatile SpiChannel spiChn = SPI_CHANNEL2; // the SPI channel to use
// volatile int spiClkDiv = 4; // 10 MHz max speed for port expander!!
// DDS units
// volatile unsigned int phase_accum_main, phase_incr_main=400.0*two32/Fs;
// DDS sine table
// #define sine_table_size 256
// volatile int sin_table[sine_table_size];


#define TIMER_LIMIT       3628/*907*/ // 40 MHz / 44100 = 907, 40 MHz / 11025 = 3628
#define DMA_CHANNEL       DMA_CHANNEL2
#define SPI_CHANNEL       SPI_CHANNEL2

// #define SCORE_SOUND_SIZE  8192 // size of +1 score sound
#define BUFFER_SIZE 512
unsigned short table[BUFFER_SIZE];
#define dma_set_transfer(table, size)  DmaChnSetTxfer(DMA_CHANNEL, table, \
                                         (void*) &SPI2BUF, size * 2, 2, 2);
#define dma_start_transfer()           DmaChnEnable(DMA_CHANNEL);
#define play_sound()             { \
  dma_set_transfer(table, BUFFER_SIZE); \
  dma_start_transfer(); \
}

static struct pt pt_serial, pt_live; // thread control structs
static struct pt pt_input, pt_output, pt_DMA_output; // UART control threads

// UART Macros

#define TIMEOUT  10

#define DEBUG  false

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

// Serial thread
static PT_THREAD (protothread_serial(struct pt *pt)) {
  PT_BEGIN(pt);

  char res[10000];

  tft_setTextColor(ILI9340_WHITE);

  uart_send("ATE0"); // disable echo
  wait_recv(res, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send("AT");
  wait_recv(res, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPMUX=0");
  wait_recv(res, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  // uart_send("AT+CIPSTART=\"TCP\",\"104.131.124.11\",3002");
  uart_send("AT+CIPSTART=\"TCP\",\"10.148.1.122\",3002");
  wait_recv(res, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPSEND=7");
  wait_recv_char(res, ">");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send_raw("start\n");
  wait_recv(res, "SEND OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);

  tft_setTextSize(2);
  tft_setTextColor(ILI9340_WHITE);

  static int table_index = 0;

  while (1) {
    static int count = 0;
    static int num_lines = 0;
    while (1) {
      uart_recv(res);
      char* ptr = strchr(res, ':');
      if (ptr) {
        unsigned int size;
        sscanf(res, "+IPD,%u:", &size);

        static short num = 0;
        static bool low = false;
        static bool high = false;
        static int i;
        for (i = ptr - res + 1; i < ptr - res + 1 + size; i++) {
          if ((res[i] >> 6) % 2 == 0) {
            num |= res[i] & 0b00111111;
            low = true;
          } else {
            num |= (res[i] & 0b00111111) << 6;
            high = true;
          }

          if (low == true && high == true) {
            table[table_index++] = DAC_config_chan_A | num;
            // table[table_index + 1] = table[table_index++]; // downsampling by 2x

            if (table_index == BUFFER_SIZE) {
              play_sound();
              table_index = 0;
            }

            num = 0;
            low = false;
            high = false;
          }
        }

        // static char num[5];
        // static int num_index = 0;
        // static int i = 0;
        // for (i = ptr - res + 1; i < ptr - res + 1 + size; i++) {
        //   if (res[i] == '|') {
        //     num[num_index] = 0;
        //     num_index = 0;
        //     table[table_index++] = DAC_config_chan_A | atoi(num);
        //     // table[table_index + 1] = table[table_index++]; // downsampling by 2x
        //     if (table_index == BUFFER_SIZE) {
        //       play_sound();
        //       table_index = 0;
        //     }
        //   } else {
        //     num[num_index++] = res[i];
        //   }
        // }

        // strncpy(res, ++ptr, size);
        // res[size] = 0;
        // char* data_point = strtok(res, "|");
        // while (data_point != NULL) {
        //   table[table_index] = DAC_config_chan_A | atoi(data_point);
        //   table_index++;

        //   if (table_index == BUFFER_SIZE) {
            
        //     tft_fillScreen(ILI9340_GREEN);
        //     int i;
        //     tft_setTextSize(1);
        //     for (i = 0; i < table_index; i++) {
        //       tft_setCursor(10, 10 + i * 10);
        //       sprintf(res, "%d", table[i]);
        //       tft_writeString(res);
        //     }
            
        //     play_sound();
        //     /*
        //     while (1) {
        //       PT_YIELD_TIME_msec(10000);
        //       play_sound();
        //     }
        //     */
        //     table_index = 0;
        //   }

        //   data_point = strtok(NULL, "|");
        // }
        break;
      }
      count++;
      if (count == TIMEOUT) {
        tft_fillScreen(ILI9340_RED);
        break;
      }
    }
  }

  while(1) {
    PT_YIELD_TIME_msec(1000);
  }

  PT_END(pt);
}

static PT_THREAD (protothread_live(struct pt *pt)) {
  PT_BEGIN(pt);

  while (1) {
    tft_fillRoundRect(10, 200, 10, 10, 1, ILI9340_BLUE);
    PT_YIELD_TIME_msec(1000);
    tft_fillRoundRect(10, 200, 10, 10, 1, ILI9340_GREEN);
    PT_YIELD_TIME_msec(1000);
  }

  PT_END(pt);
}

void main(void) {
  // SYSTEMConfigPerformance(PBCLK);

  ANSELA = 0; ANSELB = 0; 

  INTEnableSystemMultiVectoredInt(); // enable interrupts

  PT_setup(); // configure threads

  // Timer setup
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, TIMER_LIMIT);

  // SPI setup
  SpiChnOpen(SPI_CHANNEL, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | \
             SPI_OPEN_CKE_REV | SPICON_FRMEN | SPICON_FRMPOL, 2);

  // DMA setup
  DmaChnOpen(DMA_CHANNEL, 0, DMA_OPEN_DEFAULT);
  DmaChnSetEventControl(DMA_CHANNEL, DMA_EV_START_IRQ(_TIMER_2_IRQ));

  PPSOutput(2, RPB5, SDO2); // SPI -> DAC
  PPSOutput(4, RPB10, SS2); // RB9 -> DAC CS

  // init the display
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  tft_setRotation(0); // 240x320 vertical display

  // init the threads
  PT_INIT(&pt_serial);
  PT_INIT(&pt_live);

  while (1){
    PT_SCHEDULE(protothread_serial(&pt_serial));
    // PT_SCHEDULE(protothread_live(&pt_live));
  }
}
