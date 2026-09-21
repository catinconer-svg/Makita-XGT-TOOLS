#include <Arduino.h>
#include "XGTSerial.h"

#define MAKITAPIN 2 

XGTSerial makitaSerial(MAKITAPIN);

// Подсчет CRC для коротких 8-байтных пакетов
uint8_t shortcrc(uint8_t* rxBuf, uint8_t length) {
  uint16_t crc = rxBuf[0];
  for (uint8_t i = 2; i < length; i++) {
    crc += (rxBuf[i]); 
  }
  return crc;
}

// Отправка 8-байтной команды и чтение 8-байтного ответа
void send_cmd(uint8_t* rpy, uint8_t cmd, uint8_t num_args, uint8_t* args) {
  uint8_t buffer[] = { 0xCC, 0x00, cmd, 0x00, 0x00, 0x00, 0x00, 0x33 };
  memset(rpy, 0, 8);
  if (num_args > 0 && args != NULL) {
    memcpy(buffer + 3, args, num_args);
  }
  buffer[1] = shortcrc(buffer, 8);

  for (int i = 0; !(rpy[0] == 0xcc && rpy[1] == shortcrc(rpy, 8)) && i < 16; i++) {
    makitaSerial.write(buffer, 8);
    makitaSerial.read(rpy, 8);
    delay(1);
  }
  delay(4);
}

// Получение модели батареи (длинная команда)
void get_model(char* out) {
  uint8_t buffer[32];
  uint8_t cmd[] = { 0xA5, 0xA5, 0x00, 0x1A, 0x50, 0x2B, 0x4D, 0x4C, 0x00, 0xCB, 0x13, 0x07, 0x00, 0x06, 0x00, 0x03, 0x00, 0x01, 0x13, 0x0B, 0x02, 0x3B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  memset(buffer, 0, 32);
  int i = 0;
  for (; i < 9 && buffer[0] == 0; i++) {
    makitaSerial.write(cmd, 32);
    makitaSerial.read(buffer, 32);
    delay(5);
  }
  if (i != 9) {
    int idx = 32 - ((buffer[3] & 0xF) + 3);
    for (int j = 0; j < 8; j++) {
      out[j] = buffer[idx - j];
    }
  }
  out[8] = 0;
}

uint16_t num_charges() {
  uint8_t args[] = { 0x00, 0x54 };
  uint8_t rpy[8] = { 0 };
  send_cmd(rpy, 0xc0, 2, args);
  return rpy[4] | (rpy[5] << 8);
}

uint8_t get_lockout() {
  uint8_t args[] = { 0x00, 0x60 };
  uint8_t rpy[8] = { 0 };
  send_cmd(rpy, 0xc0, 2, args);
  return rpy[4];
}

uint16_t remaining_capacity() {
  uint8_t args[] = { 0x00, 0x64 };
  uint8_t rpy[8] = { 0 };
  send_cmd(rpy, 0xc0, 2, args);
  return rpy[4] | (rpy[5] << 8);
}

uint16_t cell_capacity() {
  uint8_t args[] = { 0x08 };
  uint8_t rpy[8] = { 0 };
  send_cmd(rpy, 0xDD, 1, args);
  return rpy[5] * 100;
}

uint8_t cell_parallel() {
  uint8_t args[] = { 0x0A };
  uint8_t rpy[8] = { 0 };
  send_cmd(rpy, 0xDD, 1, args);
  return rpy[4];
}

float temperature_a() {
  uint8_t args[] = { 0x03, 0x1A };
  uint8_t rpy[8] = { 0 };
  send_cmd(rpy, 0xc0, 2, args);
  return ((rpy[4] | (rpy[5] << 8)) / 10.0f) - 273.15f;
}

float temperature_b() {
  uint8_t args[] = { 0x03, 0x1C };
  uint8_t rpy[8] = { 0 };
  send_cmd(rpy, 0xc0, 2, args);
  return ((rpy[4] | (rpy[5] << 8)) / 10.0f) - 273.15f;
}

float get_voltage(int cell_idx) {
  uint8_t args[] = { 0x03, 0x00 };
  uint8_t rpy[8] = { 0 };
  args[1] = cell_idx * 2;
  send_cmd(rpy, 0xc0, 2, args);
  int16_t raw_val = rpy[4] | (rpy[5] << 8);
  return (raw_val / 1000.0f); 
}

void get_current_histogram(uint8_t *dest) {
  uint8_t args[2];
  uint8_t rpy[8];
  
  args[0] = 0x00; args[1] = 0xD8; send_cmd(rpy, 0xc0, 2, args); dest[0] = rpy[4]; dest[1] = rpy[5];
  args[1] = 0xDA; send_cmd(rpy, 0xc0, 2, args); dest[2] = rpy[4]; dest[3] = rpy[5];
  args[1] = 0xDC; send_cmd(rpy, 0xc0, 2, args); dest[4] = rpy[4]; dest[5] = rpy[5];
}

void get_temp_histogram(uint8_t *dest) {
  uint8_t args[2];
  uint8_t rpy[8];
  
  args[0] = 0x00; args[1] = 0xC0; send_cmd(rpy, 0xc0, 2, args); dest[0] = rpy[4]; dest[1] = rpy[5];
  args[1] = 0xC2; send_cmd(rpy, 0xc0, 2, args); dest[2] = rpy[4]; dest[3] = rpy[5];
  args[1] = 0xC4; send_cmd(rpy, 0xc0, 2, args); dest[4] = rpy[4]; dest[5] = rpy[5];
}

// СЕРВИСНЫЕ ФУНКЦИИ
void unlock_battery() {
  uint8_t args1[] = { 0x96, 0xA5 };
  uint8_t args2[] = { 0x2F };
  uint8_t rpy[8];
  send_cmd(rpy, 0xD9, 2, args1); delay(5);
  send_cmd(rpy, 0xD2, 1, args2); delay(5);
  Serial.println(F("\n[+] Команда разблокировки (Unlock) отправлена!"));
}

void full_calibration_reset() {
  uint8_t args_zero[] = { 0x00, 0x00 };
  uint8_t args_init[] = { 0x01, 0x01 };
  uint8_t rpy[8];
  send_cmd(rpy, 0xDE, 2, args_zero); delay(15);
  send_cmd(rpy, 0xDF, 2, args_init); delay(15);
  send_cmd(rpy, 0xD2, 1, args_zero); delay(15);
  Serial.println(F("\n[+] Полный сброс таблиц SOC и калибровки выполнен!"));
}

void draw_bar(const char* label, uint8_t value) {
  Serial.print(label);
  Serial.print(F(" ["));
  if (value < 10) Serial.print(F(" "));
  if (value < 100) Serial.print(F(" "));
  Serial.print(value);
  Serial.print(F("]: "));
  
  int bars = map(value, 0, 255, 0, 25);
  for (int i = 0; i < bars; i++) {
    Serial.print(F("=")); 
  }
  Serial.println();
}

void show_menu() {
  char model[10];
  get_model(model);

  Serial.println(F("\n=================================================="));
  Serial.print(F(" ПОДКЛЮЧЕНА БАТАРЕЯ: ")); 
  if (strlen(model) > 0) Serial.println(model); 
  else Serial.println(F("НЕИЗВЕСТНО (Проверьте линию TR)"));
  Serial.println(F("=================================================="));
  Serial.println(F("1 -> Состояние батареи (Телеметрия)"));
  Serial.println(F("2 -> Логи эксплуатации (Гистограммы нагрузки)"));
  Serial.println(F("3 -> Сервисные функции и управление"));
  Serial.println(F("=================================================="));
  Serial.print(F("Введите номер пункта меню: "));
}

void show_service_menu() {
  Serial.println(F("\n--- СЕРВИСНЫЕ ФУНКЦИИ ---"));
  Serial.println(F("1 -> Разблокировка (Unlock)"));
  Serial.println(F("2 -> Полный сброс калибровок (Hard Reset)"));
  Serial.println(F("0 -> Назад в главное меню"));
  Serial.println(F("-------------------------"));
  Serial.print(F("Выберите подпункт: "));
}

void print_telemetry() {
  Serial.println(F("\n--- СОСТОЯНИЕ БАТАРЕИ ---"));
  uint16_t base_cap = cell_capacity();
  uint8_t parallel = cell_parallel();
  Serial.print(F("Заводская емкость: ")); Serial.print(base_cap * parallel); Serial.println(F(" mAh"));
  Serial.print(F("Количество циклов: ")); Serial.println(num_charges());
  
  uint8_t lockout = get_lockout();
  Serial.print(F("Флаг блокировки (Lockout): ")); Serial.print(lockout);
  if (lockout == 0x00) Serial.println(F(" (АКБ исправен)"));
  else Serial.println(F(" (АКБ заблокирован / Ошибка)"));

  Serial.print(F("Оставшаяся емкость: ")); Serial.print(remaining_capacity()); Serial.println(F(" mAh"));
  Serial.print(F("Температура датчика 1: ")); Serial.print(temperature_a(), 1); Serial.println(F(" C"));
  Serial.print(F("Температура датчика 2: ")); Serial.print(temperature_b(), 1); Serial.println(F(" C"));
  
  // Корректный математический подсчет общего напряжения по ячейкам
  float pack_v = 0;
  float cell_v[11]; 
  
  for (int i = 1; i <= 10; i++) {
    cell_v[i] = get_voltage(i);
    if (cell_v[i] > 0.5f && cell_v[i] < 5.0f) {
      pack_v += cell_v[i];
    }
  }
  
  Serial.print(F("Общее напряжение (Pack, сумма ячеек): ")); Serial.print(pack_v, 2); Serial.println(F(" V"));
  
  Serial.println(F("Поячеистое напряжение:"));
  for (int i = 1; i <= 10; i++) {
    Serial.print(F("  Ячейка ")); Serial.print(i); Serial.print(F(": ")); 
    Serial.print(cell_v[i], 3); Serial.println(F(" V"));
  }
  Serial.println(F("-------------------------"));
}

void print_histograms() {
  uint8_t levels[6];
  
  Serial.println(F("\n--- ГИСТОГРАММА ТОКОВОЙ НАГРУЗКИ ---"));
  memset(levels, 0, 6);
  get_current_histogram(levels);
  
  for(int i = 0; i < 6; i++) {
    char label[20];
    sprintf(label, "Уровень ток %d", i + 1);
    draw_bar(label, levels[i]);
  }

  Serial.println(F("\n--- ГИСТОГРАММА ТЕМПЕРАТУРНЫХ РЕЖИМОВ ---"));
  memset(levels, 0, 6);
  get_temp_histogram(levels);
  
  for(int i = 0; i < 6; i++) {
    char label[20];
    sprintf(label, "Уровень темп %d", i + 1);
    draw_bar(label, levels[i]);
  }
  Serial.println(F("-----------------------------------------"));
}

enum MenuState { MAIN_MENU, SERVICE_MENU };
MenuState currentMenu = MAIN_MENU;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  delay(500);
  show_menu();
}

void loop() {
  if (Serial.available() > 0) {
    char input = Serial.read();
    if (input == '\n' || input == '\r') return;

    if (currentMenu == MAIN_MENU) {
      switch (input) {
        case '1':
          print_telemetry();
          show_menu();
          break;
        case '2':
          print_histograms();
          show_menu();
          break;
        case '3':
          currentMenu = SERVICE_MENU;
          show_service_menu();
          break;
        default:
          Serial.println(F("\n[!] Неверный пункт. Выберите 1, 2 или 3."));
          show_menu();
          break;
      }
    } 
    else if (currentMenu == SERVICE_MENU) {
      switch (input) {
        case '1':
          unlock_battery();
          show_service_menu();
          break;
        case '2':
          full_calibration_reset();
          show_service_menu();
          break;
        case '0':
          currentMenu = MAIN_MENU;
          show_menu();
          break;
        default:
          Serial.println(F("\n[!] Неверный подпункт."));
          show_service_menu();
          break;
      }
    }
  }
}
