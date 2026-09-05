// Arduino UNO Q - MCU 側 Sketch
// 透過 Bridge RPC 向 Linux 側提供 set_led 與 read_analog 兩個函式。
// 標頭檔名稱請以 App Lab 內建的 Bridge 範例為準。

#include <Arduino_RouterBridge.h>

const int LED_PIN = LED_BUILTIN;

bool set_led(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  return on;
}

int read_analog(int pin) {
  // pin 為 0..5，對應 A0..A5
  static const int pins[] = {A0, A1, A2, A3, A4, A5};
  if (pin < 0 || pin > 5) {
    return -1;
  }
  return analogRead(pins[pin]);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Bridge.begin();
  Bridge.provide("set_led", set_led);
  Bridge.provide("read_analog", read_analog);
}

void loop() {
  // 即時邏輯（例如 PID、安全互鎖）可放在這裡。
  // 本範例所有動作皆由 Linux 側透過 RPC 觸發。
  delay(10);
}
