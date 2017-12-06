/*
 * File:        main.c
 * Author:      Norman Chen (nyc7), Giacomo DiLiberto (gvd8), Ram Vellanki (rsv32)
 * For use with Sean Carroll's Big Board or Small Board
 * Target PIC:  PIC32MX250F128B
 */

#include "config.h" // clock AND ProtoThreads configuration
#include "pt_cornell_1_2_2.h" // threading library

// graphics libraries
#include "tft_master.h"
#include "tft_gfx.h"
#include "utils/tft/tft_master.h"

// utility libraries
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "alarm.h" // alarm sound header file

#define IP_ADDRESS  "104.131.124.11" // remote server IPv4 address

#define DISTANCE_THRESHOLD  150 // threshold in ADC units for distance sensor

#define DAC_config_chan_A 0b0011000000000000
#define DAC_config_chan_B 0b1011000000000000

#define TIMER_LIMIT       3628 // 40 MHz / 11025 = 3628
#define DMA_CHANNEL       DMA_CHANNEL2
#define SPI_CHANNEL       SPI_CHANNEL2

// DMA Sound Macro Functions
#define dma_set_transfer(table, size)  DmaChnSetTxfer(DMA_CHANNEL, table, \
                                         (void*) &SPI2BUF, size * 2, 2, 2);
#define dma_start_transfer()           DmaChnEnable(DMA_CHANNEL);
#define sound_alarm()                  dma_set_transfer(ALARM_SOUND, ALARM_SOUND_SIZE); dma_start_transfer();
#define stop_alarm()                   DmaChnAbortTxfer(DMA_CHANNEL);

static struct pt pt_main, pt_receive; // thread control structs
static struct pt pt_input, pt_output, pt_DMA_output; // UART control threads

// UART Macros

#define TIMEOUT          10   // number of UART receives to retrieve appropriate before bailing out
#define RECEIVE_TIMEOUT  50   // timeout in ms for receiving a response after a send
#define ACCURATE         true // wait for exact responses from the esp if true

// debug enable flags
#define DEBUG  false
#define SENSOR_DEBUG  false

// sends `msg` over UART appending with a carriage return and line feed
#define uart_send(msg) { \
  sprintf(PT_send_buffer, msg "\r\n"); \
  PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output)); \
}

// sends `msg` over UART
#define uart_send_raw(msg) { \
  sprintf(PT_send_buffer, msg); \
  PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output)); \
}

// reads and stores the next line in the UART buffer into the `res` pointer
#define uart_recv(res) { \
  PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input)); \
  sprintf(res, PT_term_buffer); \
}

// reads and stores a single character from the UART buffer into the `res` pointer
#define uart_recv_char(res) { \
  PT_SPAWN(pt, &pt_input, PT_GetSerialBufferChar(&pt_input)); \
  sprintf(res, PT_term_buffer); \
}

// continuously reads the UART buffer until the value is equal to `msg`
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

// continuously reads the UART buffer until the value is equal to `msg1` or `msg2`
#define wait_recv_either(res, msg1, msg2) { \
  res[0] = 0; \
  int count = 0; \
  int num_lines = 0; \
  while (strcmp(res, msg1) != 0 && strcmp(res, msg2) != 0) { \
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

// continuously reads the UART buffer character by character until the character is `msg`
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

volatile bool start = false; // asynchronous receive thread enable

// main thread for initializing the module and TCP connection, reading sensor
// data, and sending the appropriate data up to the server via UART to the module
static PT_THREAD (protothread_main(struct pt *pt)) {
  PT_BEGIN(pt);

  char buffer[32];

  // close any existing connection
  uart_send("AT+CIPCLOSE");
  wait_recv_either(buffer, "OK", "ERROR");
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  // initialize esp and TCP connection
  uart_send("ATE0"); // disable echo
  if (ACCURATE) {
    wait_recv(buffer, "OK");
  } else {
    PT_YIELD_TIME_msec(RECEIVE_TIMEOUT);
  }
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  // check if esp8266 responds to AT commands
  uart_send("AT");
  if (ACCURATE) {
    wait_recv(buffer, "OK");
  } else {
    PT_YIELD_TIME_msec(RECEIVE_TIMEOUT);
  }
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  // configures esp8266 for single ip connection
  uart_send("AT+CIPMUX=0");
  if (ACCURATE) {
    wait_recv(buffer, "OK");
  } else {
    PT_YIELD_TIME_msec(RECEIVE_TIMEOUT);
  }
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);
  // connect the esp8266 to the web server
  uart_send("AT+CIPSTART=\"TCP\",\""IP_ADDRESS"\",3002");
  if (ACCURATE) {
    wait_recv(buffer, "OK");
  } else {
    PT_YIELD_TIME_msec(RECEIVE_TIMEOUT);
  }
  if (DEBUG) tft_fillScreen(ILI9340_BLACK);

  start = true; // enable asynchronous receiving

  static bool edge = true;
  while (1) {
    // read the distance sensor value
    const unsigned long int value = ReadADC10(0);
    if (SENSOR_DEBUG) {
      tft_fillRoundRect(10, 10, 100, 100, 1, ILI9340_BLACK);
      tft_setCursor(10, 10);
      sprintf(buffer, "%d", value);
      tft_writeString(buffer);
    }
    if (value < DISTANCE_THRESHOLD) {
      if (edge == false) {
        // detected falling edge, send "event" to server
        edge = true;
        uart_send("AT+CIPSEND=7");
        PT_YIELD_TIME_msec(10);
        if (DEBUG) tft_fillScreen(ILI9340_BLACK);
        uart_send_raw("event\n");
        PT_YIELD_TIME_msec(10);
        if (DEBUG) tft_fillScreen(ILI9340_BLACK);
      }
    } else {
      edge = false;
    }
    PT_YIELD_TIME_msec(10);
  }

  PT_END(pt);
}

static PT_THREAD (protothread_receive(struct pt *pt)) {
  PT_BEGIN(pt);

  char buffer[32];

  // only start this asynchronous receiving once start is true (i.e. the 
  // initialization setup steps are completed by the sending thread)
  while (!start) {
    PT_YIELD_TIME_msec(10);
  }

  static bool alarmed = false; // true if alarm sound has been started
  while (1) {
    uart_recv(buffer);
    if (strchr(buffer, '!')) { // sound the alarm
      if (!alarmed) sound_alarm();
      alarmed = true;
      break;
    } else if (strchr(buffer, '-')) { // stop the alarm
      if (alarmed) stop_alarm();
      alarmed = false;
      break;
    }
  }

  PT_END(pt);
}

void main(void) {
  ANSELA = 0; ANSELB = 0; 

  INTEnableSystemMultiVectoredInt(); // enable interrupts

  PT_setup(); // configure threads

  // Timer setup
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, TIMER_LIMIT);

  // SPI setup
  SpiChnOpen(SPI_CHANNEL, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | \
             SPI_OPEN_CKE_REV | SPICON_FRMEN | SPICON_FRMPOL, 2);
  // DMA setup
  DmaChnOpen(DMA_CHANNEL, 0, DMA_OPEN_AUTO); // auto for repeating alarm sound
  DmaChnSetEventControl(DMA_CHANNEL, DMA_EV_START_IRQ(_TIMER_2_IRQ));

  PPSOutput(2, RPB5, SDO2); // SPI -> DAC
  PPSOutput(4, RPB10, SS2); // RB9 -> DAC CS

  // ADC setup
  CloseADC10(); // ensure the ADC is off before setting the configuration
  // define setup parameters for OpenADC10
  // Turn module on | ouput in integer | trigger mode auto | enable autosample
  // ADC_CLK_AUTO
  //  - Internal counter ends sampling and starts conversion (Auto convert)
  // ADC_AUTO_SAMPLING_ON
  //  - sampling begins immediately after last conversion completes
  /// - SAMP bit is automatically set
  // ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
  #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON
  // define setup parameters for OpenADC10
  // ADC ref external  | disable offset test | disable scan mode | do 2 sample |
  //                   use single buf | alternate mode on
  #define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | \
                  ADC_SAMPLES_PER_INT_2 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_ON
  // Define setup parameters for OpenADC10
  // use peripherial bus clock | set sample time | set ADC clock divider
  // ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
  // ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
  // SLOW it down a little
  #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_15 | ADC_CONV_CLK_Tcy
  // define setup parameters for OpenADC10
  // set AN11 and  as analog inputs
  #define PARAM4 ENABLE_AN11_ANA | ENABLE_AN5_ANA
  // define setup parameters for OpenADC10
  // do not assign channels to scan
  #define PARAM5 SKIP_SCAN_ALL
  // configure to sample AN11 on MUX A
  SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11 );
  // configure ADC using the parameters defined above
  OpenADC10( PARAM1, PARAM2, PARAM3, PARAM4, PARAM5 );
  EnableADC10(); // Enable the ADC

  // init the display
  if (DEBUG) {
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    tft_setTextSize(2);
    tft_setTextColor(ILI9340_WHITE);
    tft_setRotation(0); // 240x320 vertical display
  }

  // init the threads
  PT_INIT(&pt_main);
  PT_INIT(&pt_receive);

  while (1) {
    PT_SCHEDULE(protothread_main(&pt_main));
    PT_SCHEDULE(protothread_receive(&pt_receive));
  }
}
