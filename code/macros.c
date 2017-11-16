#define uart_send(msg) { \
  sprintf(PT_send_buffer, msg); \
  PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output)); \
  PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input)); \
}

#define esp_mux() {
  uart_send("AT+CIPMUX=0"); \
}

#define esp_send(msg) { \
  int length = strlen(msg); \
  char str[12 + length]; \
  sprintf(str, "AT+CIPSEND=0,%d", strlen(msg)); \
  uart_send(str); \
}

#define esp_close() { \
  uart_send("AT+CIPCLOSE=0"); \
}

#define set_wifi_mode(mode) { \
  char str[14]; \
  sprintf(str, "AT+CWMODE=%d", mode); \
  uart_send(str); \
}

#define connect_ap(ssid, pwd) { \
  char str[10+length+strlen(ssid) + strlen(pwd)]; \
  sprintf(str, "AT+CWJAP=%s,%s", ssid, pwd); \
  uart_send(str); \
}

#define set_wifi_mode(mode) { \
  char str[14]; \
  sprintf(str, "AT+CWMODE=%d", mode); \
  uart_send(str); \
}

#define esp_start(type, addr, port) { \
  char str[17+strlen(addr)+strlen(port)]; \
  sprintf(str, "AT+CIPSTART=0,%d,%s,%s", type, addr, port); \
  uart_send(str); \
}