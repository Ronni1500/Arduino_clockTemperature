#include <Adafruit_GFX.h>     // Библиотека для работы с матрицей
#include <Adafruit_BMP085.h>  // Библиотека для датчика bmp180
#include <Max72xxPanel.h>     // Библиотека для работы с max7219
#include <Wire.h>             // Библиотека для работы с I2C устройствами
#include <Thinary_AHT10.h>    // Библиотека для датчика aht10
#include <RTClib.h>           // Бибилотека для DS3231



#define BTN_MODE 3 // Пин кнопки перехода в настройки
#define BTN_PLUS 4 // Пин кнопки плюс
#define BTN_MINUS 5 // Пин кнопки минус

RTC_DS3231 rtc; // Связываем объект clock с бибилиотекой ds3231
AHT10Class AHT10;
Adafruit_BMP085 bmp;// Объявляем переменную для доступа к SFE_BMP180 


/*Переменные*/
uint32_t timeTimer;
int in_H, in_M;
long clkTime = 0, dotTime = 0;
int  dots = 0;
byte del=0;
int offset=1,refresh=0;
int jday = 3,jnight = 0;
int pinCS = 10; // Подключение пина CS
int numberOfHorizontalDisplays = 4; // Количество светодиодных матриц по Горизонтали
int numberOfVerticalDisplays = 1; // Количество светодиодных матриц по Вертикали
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);
int wait = 50; // скорость бегущей строки
int spacer = 2;
int width = 5 + spacer; // Регулируем расстояние между символами
String mesyc = " ";
boolean clock_state;

//Переменные для вычисления погоды
boolean wake_flag;
unsigned long pressure, aver_pressure, pressure_array[6], time_array[6];
unsigned long sumX, sumY, sumX2, sumXY;
float a, b;
int delta, sleep_count;
int dispRain, count_work;



void setup(){
  Serial.begin(9600);
   bmp.begin(BMP085_ULTRAHIGHRES);  // включить датчик bmp180
   AHT10.begin(eAHT10Address_Low);  // включить датчик aht10

   // проверка RTC
  if (rtc.begin()) {
    Serial.println("RTC OK");
    clock_state = true;
  } else {
    Serial.println("RTC ERROR");
    clock_state = false;
  }
   // начальные координаты матриц 8*8
  matrix.setRotation(0, 1);        // 1 матрица
  matrix.setRotation(1, 1);        // 2 матрица
  matrix.setRotation(2, 1);        // 3 матрица
  matrix.setRotation(3, 1);        // 4 матрица

  // Находим первое средние значение для для давоения
  pressure = aver_sens();          // найти текущее давление по среднему арифметическому
  for (byte i = 0; i < 6; i++) {   // счётчик от 0 до 5
    pressure_array[i] = pressure;  // забить весь массив текущим давлением
    time_array[i] = i;             // забить массив времени числами 0 - 5
  }
  wake_flag = 1; // Первый вывод затем просчет больше не нужен
  sleep_count = 0;
  help();   // вывод текстового меню
  timeTimer = 60000;    // искусственно переполнить таймер времени,
}
void help() {
  Serial.println(F("***********************************************************************"));
  Serial.println(F("Welcome to ServoClock by AlexGyver! You can use serial commands:"));
  Serial.println(F("<set-time HH:MM>   - set time (example: set-time 01:20)"));
  Serial.println(F("<get-time>         - return RTC time"));
  Serial.println(F("<set-i-day>        - set intensive on day"));
  Serial.println(F("<set-i-night>      - set intensive on night"));
  Serial.println(F("<help>             - print this instruction again"));
  Serial.println(F("***********************************************************************"));
}

void loop(){
  serialTick();     // обработка команд из порта
  // тут делаем линейную аппроксимацию для предсказания погоды
    if (wake_flag) {
      pressure = aver_sens();                          // найти текущее давление по среднему арифметическому
      for (byte i = 0; i < 5; i++) {                   // счётчик от 0 до 5 (да, до 5. Так как 4 меньше 5)
        pressure_array[i] = pressure_array[i + 1];     // сдвинуть массив давлений КРОМЕ ПОСЛЕДНЕЙ ЯЧЕЙКИ на шаг назад
      }
      pressure_array[5] = pressure;                    // последний элемент массива теперь - новое давление;
      sumX = 0;
      sumY = 0;
      sumX2 = 0;
      sumXY = 0;
      for (int i = 0; i < 6; i++) {                    // для всех элементов массива
        sumX += time_array[i];
        sumY += (long)pressure_array[i];
        sumX2 += time_array[i] * time_array[i];
        sumXY += (long)time_array[i] * pressure_array[i];
      }
       delay(300);
      a = 0;
      a = (long)6 * sumXY;             // расчёт коэффициента наклона приямой
      a = a - (long)sumX * sumY;
      a = (float)a / (6 * sumX2 - sumX * sumX);
      delta = a * 6;      // расчёт изменения давления
      dispRain = map(delta, -250, 250, 100, -100);  // пересчитать в проценты
      wake_flag = 0;
    }
    
    if(millis()-clkTime > 35000 && !del && dots) { //каждые 15 секунд запускаем бегущую строку
      sleep_count++; // Каждые 15 секунд прибавляем 
      if (sleep_count >= 45) {  // Если общее время сна больше примерно 10 мин то открываем флаг для просчета вероятности изменения погоды
        wake_flag = 1;          // рарешить выполнение расчета
        sleep_count = 0;        // обнулить счетчик
      }
        //Бегущая строка 
        ScrollText(utf8rus(mesyc+"Температура"+AHT10.GetTemperature()+"C Влажность "+AHT10.GetHumidity()+"%" + "Давление " + bmp.readPressure()/133.322 + "мм.рт.ст." + "Погода"+dispRain+"%")); //тут текст строки, потом будет погода и т.д.
        clkTime = millis(); // Получаем количество миллисекунд
    }
    oclock();
    if(millis()-dotTime > 500) {
     dotTime = millis();
     dots = !dots;
    }
}


/*Функция вывода времени на матрицу*/
void oclock() {
    matrix.fillScreen(LOW);
    int y = (matrix.height() - 8) / 2; // Центрируем текст по Вертикали
    DateTime now = rtc.now();

    in_H = now.hour();// Получаем час из rtc
    in_M = now.minute();// Получаем минуты из rtc
    if(now.second() & 1){matrix.drawChar(14, y, (String(":"))[0], HIGH, LOW, 1);} //каждую четную секунду печатаем двоеточие по центру (чтобы мигало)
    else{matrix.drawChar(14, y, (String(" "))[0], HIGH, LOW, 1);}
    String hour1 = String (in_H/10); // десятки часов
    String hour2 = String (in_H%10); // единицы часов
    String min1 = String (in_M/10); // десятки минут
    String min2 = String (in_M%10); // единицы минут
     int xh = 2;
     int xm = 19;
    matrix.drawChar(xh, y, hour1[0], HIGH, LOW, 1); //Выводим десятки часов
    matrix.drawChar(xh+6, y, hour2[0], HIGH, LOW, 1); //Выводим единицы часов
    matrix.drawChar(xm, y, min1[0], HIGH, LOW, 1); // Выводим десятки минут
    matrix.drawChar(xm+6, y, min2[0], HIGH, LOW, 1); // Выводим есдиницы  мынут
    if (in_H >= 6 && in_H < 23) {
        matrix.setIntensity(jday); //Адаптивная подстветка для дня
    }
    else {
       matrix.setIntensity(jnight); //Адаптивная подстветка для ночи
    }
     matrix.write(); // Вывод на диспей
}
/*Функия скролла для матрицы*/
void ScrollText (String text){
    for ( int i = 0 ; i < width * text.length() + matrix.width() - 1 - spacer; i++ ) {
    if (refresh==1) i=0;
    refresh=0;
    matrix.fillScreen(LOW);
    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // Центрируем текст по Вертикали
 
    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if ( letter < text.length() ) {
        matrix.drawChar(x, y, text[letter], HIGH, LOW, 1);
      }
      letter--;
      x -= width;
    }
    matrix.write(); // Вывод на дисплей
    delay(wait);
  }
}
/*Функция простого вывода любого текста*/
String utf8rus(String source)
{
  int i,k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
          n = source[i]; i++;
          if (n == 0x81) { n = 0xA8; break; }
          if (n >= 0x90 && n <= 0xBF) n = n + 0x30-1;
          break;
        }
        case 0xD1: {
          n = source[i]; i++;
          if (n == 0x91) { n = 0xB8; break; }
          if (n >= 0x80 && n <= 0x8F) n = n + 0x70-1;
          break;
        }
      }
    }
    m[0] = n; target = target + String(m);
  }
return target;
}
/*Функция для определения срден арифметического для давления*/
long aver_sens() {
  pressure = 0;
  for (byte i = 0; i < 10; i++) {
    pressure += bmp.readPressure();
  }
  aver_pressure = pressure / 10;
  return aver_pressure;
}

// опросчик и парсер сериал
void serialTick() {
  if (Serial.available() > 0) {
    String buf = Serial.readString();
    if (buf.startsWith("set-time")) {    // set-time 01:20
      int newH = buf.substring(9, 11).toInt();
      int newM = buf.substring(12, 14).toInt();
      if (newH >= 0 && newH <= 23 && newM >= 0 && newM <= 59) {
        rtc.adjust(DateTime(2012, 21, 12, newH, newM, 0));
        timeTimer = 60000 + millis();
        Serial.print(F("RTC time set to "));
        Serial.print(newH); Serial.print(":"); Serial.println(newM);
      } else {
        Serial.println(F("Wrong value!"));
      }
    }
    else if (buf.startsWith("get-time")) {
      Serial.print(F("RTC time is "));
      DateTime now = rtc.now();
      Serial.print(now.hour()); Serial.print(":"); Serial.println(now.minute());
    }
    else if (buf.startsWith("set-i-day")) {
      jday = buf.substring(10, 12).toInt();
    }
    else if (buf.startsWith("set-i-night")) {
      jnight = buf.substring(11, 13).toInt();
    }
    if (buf.startsWith("help")) {
      help();
    }
  }
}
