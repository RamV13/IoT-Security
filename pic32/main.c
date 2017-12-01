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


#define TIMER_LIMIT       907/*907*/ // 40 MHz / 44100 = 907, 40 MHz / 11025 = 3628
#define DMA_CHANNEL       DMA_CHANNEL2
#define SPI_CHANNEL       SPI_CHANNEL2

// #define SCORE_SOUND_SIZE  8192 // size of +1 score sound
#define RES_SIZE    2000
#define NUM_RES     5
char res_table[NUM_RES][RES_SIZE];
volatile bool res_valids[NUM_RES];
unsigned int read_res_index;
unsigned int write_res_index;

#define BUFFER_SIZE 512
#define NUM_TABLES  10
unsigned short tables[NUM_TABLES][BUFFER_SIZE];
volatile bool valids[NUM_TABLES];
unsigned short play_table_index = 1;
unsigned short write_table_index;
bool playing = false;

#define dma_set_transfer(table, size)  DmaChnSetTxfer(DMA_CHANNEL, table, \
                                         (void*) &SPI2BUF, size * 2, 2, 2);
#define dma_start_transfer()           DmaChnEnable(DMA_CHANNEL);
#define play_sound(table)             { \
  dma_set_transfer(table, BUFFER_SIZE); \
  dma_start_transfer(); \
}

static struct pt pt_serial, pt_parse, pt_dma; // thread control structs
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

  // initialize esp and TCP connection
  char buffer[32];
  uart_send("ATE0"); // disable echo
  wait_recv(buffer, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send("AT");
  wait_recv(buffer, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPMUX=0");
  wait_recv(buffer, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  // uart_send("AT+CIPSTART=\"TCP\",\"104.131.124.11\",3002");
  uart_send("AT+CIPSTART=\"TCP\",\"10.148.12.169\",3002");
  wait_recv(buffer, "OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send("AT+CIPSEND=7");
  wait_recv_char(buffer, ">");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  uart_send_raw("start\n");
  wait_recv(buffer, "SEND OK");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);

  while (1) {
    uart_recv(res_table[write_res_index]);
    res_valids[write_res_index++] = true;
    if (write_res_index == NUM_RES) write_res_index = 0;
  }

  PT_END(pt);
}

static PT_THREAD (protothread_parse(struct pt *pt)) {
  PT_BEGIN(pt);

  static unsigned int table_index = 0;
  static int timeout_count;

  while (1) {
    PT_YIELD_UNTIL(pt, res_valids[read_res_index]);
    char* ptr = strchr(res_table[read_res_index], ':');
    if (ptr) {
      unsigned int size;
      sscanf(res_table[read_res_index], "+IPD,%u:", &size);

      static short num = 0;
      static bool low = false;
      static bool high = false;
      static int i;
      for (i = ptr - res_table[read_res_index] + 1; i < ptr - res_table[read_res_index] + 1 + size; i++) {
        if ((res_table[read_res_index][i] & 0b01000000) == 0) {
          num |= res_table[read_res_index][i] & 0b00111111;
          low = true;
        } else {
          num |= (res_table[read_res_index][i] & 0b00111111) << 6;
          high = true;
        }

        if (low == true && high == true) {
          tables[write_table_index][table_index++] = DAC_config_chan_A | num;

          if (table_index == BUFFER_SIZE) {
            if (playing == false) {
              play_sound(tables[0]);
              playing = true;
            }
            table_index = 0;
            valids[write_table_index++] = true;
            if (write_table_index == NUM_TABLES) write_table_index = 0;
          }

          num = 0;
          low = false;
          high = false;
        }
      }
      break;
    }

    valids[read_res_index++] = false;
    if (read_res_index == RES_SIZE) read_res_index = 0;

    timeout_count++;
    if (timeout_count == TIMEOUT) {
      tft_fillScreen(ILI9340_RED);
      PT_EXIT(pt);
      break;
    }
  }

  PT_END(pt);
}

static PT_THREAD (protothread_dma(struct pt *pt)) {
  PT_BEGIN(pt);

  while (1) {
    PT_YIELD_UNTIL(pt, DmaChnGetEvFlags(DMA_CHANNEL) & DMA_EV_BLOCK_DONE);
    DmaChnClrEvFlags(DMA_CHANNEL, DMA_EV_BLOCK_DONE);
    PT_YIELD_UNTIL(pt, valids[play_table_index]);
    play_sound(tables[play_table_index]);
    valids[play_table_index++] = false;
    if (play_table_index == NUM_TABLES) play_table_index = 0;
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
  DmaChnSetEvEnableFlags(DMA_CHANNEL, DMA_EV_BLOCK_DONE);
  DmaChnClrEvFlags(DMA_CHANNEL, DMA_EV_BLOCK_DONE);

  PPSOutput(2, RPB5, SDO2); // SPI -> DAC
  PPSOutput(4, RPB10, SS2); // RB9 -> DAC CS

  // init the display
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  tft_setTextSize(2);
  tft_setTextColor(ILI9340_WHITE);
  tft_setRotation(0); // 240x320 vertical display

  // init the threads
  PT_INIT(&pt_serial);
  PT_INIT(&pt_parse);
  PT_INIT(&pt_dma);

  while (1){
    PT_SCHEDULE(protothread_serial(&pt_serial));
    PT_SCHEDULE(protothread_parse(&pt_parse));
    PT_SCHEDULE(protothread_dma(&pt_dma));
  }
}
