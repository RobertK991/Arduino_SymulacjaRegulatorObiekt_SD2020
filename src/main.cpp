#include <Arduino.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ILI9341.h> // Hardware-specific library 
#include <SPI.h>   
#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
//definicje do wyswietlacza, piny
// #define TFT_DC 9         //6
// #define TFT_CS 10         //7
// #define TFT_RST 8        //5
// #define TFT_MISO 12       //50
// #define TFT_MOSI 11        // 51
// #define TFT_CLK 13         //52
#define TFT_DC 6         //6
#define TFT_CS 7         //7
#define TFT_RST 5        //5
#define TFT_MISO 50       //50
#define TFT_MOSI 51        // 51
#define TFT_CLK 52         //52
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST); //Zainicjowanie wyświetlacza
//Definicje do wyświetlania poszczególnych parametrów
#define Kp "Kp="
#define Ti "Ti="
#define Td "Td="
#define Wz "Wz="

//Zmienne do enkodera
unsigned long time = 0;             //czas
volatile long pozycjaEnkodera = 0;  //Pozycja biezaca enkodera
#define pinA         2              //Pin A - przerwanie
#define pinB         3              //Pin B

//Zmienne przycisku
int przyciskPin = 4;            //pin przycisku
int poziomMenu = 0;             //poziom menu 

//Zmienne regulatora
float sygnalSterujacy = 1;      //Wasrtosc sygnalu steruajcego
float wartoscBiezaca = 0;                //Wartosc bieżąca sygnału wyjścia z obiektu
struct parametr
  {
    float KP;
    float TI;
    float TD;
    float WZ;
  };
struct lcd_obiekt
  {
    float uchyb;
    float sygnalSterujacy;
    float wartoscBiezaca;
    float WZ;
  };
//Zadania
void TaskEnkoder( void *pvParameters ); //Zadanie wyświetlania
void TaskButton( void *pvParameters);       //zadanie przycisku-zmiana poziomu menu
void TaskRegulator( void *pvParameters );   //Zadanie regulatora
void TaskObiekt( void *pvParameters );      //Zadanie obiektu
void TaskLCD( void *pvParameters ); 
//Funkcje
void wyswietlanie(int x,int y, float wartosc);    //Funkcja wyświetlania menu
void isr();                                 //Funkcja obsługująca przerwanie

//Koeljki
QueueHandle_t regulatorQueue;
// QueueHandle_t lcd_parametryKPQueue;
// QueueHandle_t lcd_parametryTIQueue;
// QueueHandle_t lcd_parametryTDQueue;
// QueueHandle_t lcd_parametryWZQueue;
QueueHandle_t lcd_parametryQueue;
QueueHandle_t lcd_regulatorQueue;
//Semafory i mutexy
SemaphoreHandle_t serial_mutex; 
/*
----------------------------------------------------------------------------------------------------------------
************************************************Funkcja Setup***************************************************
----------------------------------------------------------------------------------------------------------------
*/
void setup() 
{
  Serial.begin (9600);                                          //Zainicjowanie połączenia szeregowego
  pinMode (przyciskPin,INPUT);                                  //Zainicjowanie pinu przycisku
  regulatorQueue = xQueueCreate(16, sizeof(parametr));               //Utworzenie kolejki dla zmiennych regulatora
  // lcd_parametryKPQueue = xQueueCreate(16, sizeof(float));
  // lcd_parametryTIQueue = xQueueCreate(16, sizeof(float));
  // lcd_parametryTDQueue = xQueueCreate(16, sizeof(float));
  // lcd_parametryWZQueue = xQueueCreate(16, sizeof(float));
  lcd_parametryQueue = xQueueCreate(16, sizeof(parametr));
  lcd_regulatorQueue = xQueueCreate(16, sizeof(lcd_obiekt));
  if(regulatorQueue == NULL)                                    //Sprawdzenie czu utworzono kolejkę
  {                     
    Serial.println("serialQueue nie może zostać utworzona");    //Komunikat o błedzie
    while(true);
  }
  serial_mutex = xSemaphoreCreateMutex();                       //Utworzenie mutexu
  if (serial_mutex == NULL)                                     //Sprawdzenie czy utworzono mutex
  { 
    Serial.println("Mutex serial_mutex nie może zostać utworzony");     //Komunikat o błędzie
  }
  //Zmienne przerwań enkodera
  pinMode(pinA, INPUT);                                     //Zainicjowanie pinu enkodera A
  pinMode(pinB, INPUT);                                     //Zainicjowanie pinu enkodera B
  attachInterrupt(digitalPinToInterrupt(pinA), isr, RISING);  //Utworzenie obsługi przerwania
  time = millis();                                            //Zapisanie czasu w zmiennej
  //Tworzenie zadań
  xTaskCreate(TaskEnkoder, "ZadanieEnkodera", 512,  NULL, 2, NULL);   //Zadanie enkodera
  xTaskCreate(TaskButton, "Przycisk", 128, NULL, 3, NULL );               //Zadanie przycisku 
  xTaskCreate(TaskRegulator, "Regulator", 512, NULL, 1, NULL);
  xTaskCreate(TaskObiekt, "Obiekt", 512, NULL, 1, NULL);
  xTaskCreate(TaskLCD, "Wyświetlacz", 512, NULL, 3, NULL);
  vTaskStartScheduler();              //Harmonogramowanie zadań
}

void loop()
{
///Puste
}
/*
----------------------------------------------------------------------------------------------------------------
************************************************Zadanie Enkodera************************************************
----------------------------------------------------------------------------------------------------------------
*/
void TaskEnkoder(void *pvParameters)  //Zadanie enkodera
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  float pozycjaEnkoderaOld = 0;        //Stara pozycja enkodera
  (void) pvParameters;
  tft.begin();                                          //  Inicjalizuje wyświetlacz
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);       //Ustawia kolor napisów
  tft.setTextSize(2);                                   //Ustawia wielkość napisów
  tft.fillScreen(ILI9341_BLACK);                        //Zaczernia ekran
  tft.setRotation(3);                                   //Zmiana orientacji wyświetlania na poziomą
  tft.drawLine(0, 40, tft.width(), 40, ILI9341_WHITE);  //Rysuje linie poziomą u góry ekranu
  tft.setCursor(0, 1);      //Napis kp
  tft.print(Kp);          
  tft.setCursor(160, 1);     //Napis Ti
  tft.print(Ti);
  tft.setCursor(0, 19);    //Napis Td
  tft.print(Td);
  tft.setCursor(160, 19);    //Napis WZ
  tft.print(Wz);
  tft.setCursor(0, 100);
  tft.print("E: ");
  tft.setCursor(0, 150);
  tft.print("U: ");
  tft.setCursor(0, 200);
  tft.print("Y: ");
  //Zmienne regulatora
  parametr parametry;
  (void) pvParameters;
  for (;;)                //Pętal zadania
  { 
    //xSemaphoreTake( serial_mutex, portMAX_DELAY );
    /*
    if (poziomMenu == 0)
    pozycjaEnkoderaOld = KP;
    else if (poziomMenu == 1)
    pozycjaEnkoderaOld = TI;
    else if (poziomMenu == 2)
    pozycjaEnkoderaOld = TD;
    else 
    pozycjaEnkoderaOld = 0;
    */
    while (pozycjaEnkoderaOld != pozycjaEnkodera)   //Sprawdza czy nastąpiła zmiana w enkoderze
    {
      pozycjaEnkoderaOld = pozycjaEnkodera;         //Aktualizacja starej pozycji enkodera
    } 
    switch (poziomMenu)                             //Sprawdzenie którą zmienną nadpisywać
    {
      case 0:                                       //Zmiana zmiennej Kp
      parametry.KP = (pozycjaEnkoderaOld/10); 
      if (parametry.KP<=0)                                    //Ograniczenia zmiennej
      {
        parametry.KP = 0;
      }
      // Serial.println(parametry.KP);
      wyswietlanie(40, 1, parametry.KP);                 //Wyświetlanie zmiennej
      // xQueueSend(lcd_parametryKPQueue, &parametry.KP, portMAX_DELAY);
      // xQueueSend(lcd_parametryQueue, &parametry, portMAX_DELAY);
      break;
      case 1:
      parametry.TI = (pozycjaEnkoderaOld/10); 
      if (parametry.TI<=0)                                    //Ograniczenia zmiennej
      {
        parametry.TI = 0;
      }
      wyswietlanie(200, 1, parametry.TI);                 //Wyświetlanie zmiennej
      // xQueueSend(lcd_parametryTIQueue, &parametry.TI, portMAX_DELAY);
      //  xQueueSend(lcd_parametryQueue, &parametry, portMAX_DELAY);
      break;
      case 2:                                       //Zmiana zmiennej Ti
      parametry.TD = (pozycjaEnkoderaOld/10); 
      if (parametry.TD<=0)                                    //Ograniczenia zmiennej
      {
        parametry.TD = 0;
      }
      wyswietlanie(40, 19, parametry.TD);                 //Wyświetlanie zmiennej
      // xQueueSend(lcd_parametryTDQueue, &parametry.TD, portMAX_DELAY);
      //  xQueueSend(lcd_parametryQueue, &parametry, portMAX_DELAY);
      break;
      case 3:                                      //Zmiana zmiennej wartosci zadanej
      parametry.WZ = pozycjaEnkoderaOld;
      if (parametry.WZ<=0) {parametry.WZ = 0;}        //Dolne ograniczenie zmiennej
      if (parametry.WZ>=100) {parametry.WZ = 100;}    //Górne ograniczenie zmiennej
      wyswietlanie(200, 19, parametry.WZ);                 //Wyświetlanie zmiennej
      // xQueueSend(lcd_parametryWZQueue, &parametry.WZ, portMAX_DELAY);
      //  xQueueSend(lcd_parametryQueue, &parametry, portMAX_DELAY);
      break;
      case 4:                                       //Zmiana zmiennej Td
      tft.drawLine(0, 38, tft.width(),38, ILI9341_BLACK);  //Czerni starą linię   
      xQueueSend(regulatorQueue, &parametry, portMAX_DELAY);   //Wysyła parametry do regulatora
      poziomMenu = 5;
      break;
      default:
      break;
    }
    //xSemaphoreGive( serial_mutex );
    //vTaskDelay(50/portTICK_PERIOD_MS);    
    vTaskDelayUntil( &xLastWakeTime, 100/portTICK_PERIOD_MS);
  }
}
/*
----------------------------------------------------------------------------------------------------------------
************************************************Zadanie Przycisku***********************************************
----------------------------------------------------------------------------------------------------------------
*/
void TaskButton( void *pvParameters)
{
  (void) pvParameters;
  int oldStanPin = LOW;         //Zmienna starej wartośći pinu
  int value = LOW;              //Zmienna nowej wartośći pinu     
  float timeStart = 0;          //Czas startu
  float timeStop = 0;           //Czas stopu
  float timeRoznica = 0;        //Różnica czasów
  for (;;)                      //Pętla zadania
  {
    //xSemaphoreTake( serial_mutex, portMAX_DELAY );
    value=digitalRead(przyciskPin);       //Odczyt wartości pinu
    if (value == HIGH && oldStanPin == LOW)       //Sprawdzenie czy został kliknięty
    {
      timeStart = millis();                       //Aktualizacja czasu
      oldStanPin = HIGH;                          //Ustawia starą wartość pinu
    }
    if (value == LOW && oldStanPin == HIGH)       //Sprawdza czy został puszczony 
    {
      timeStop = millis();                        //Aktualizacja czasu puszczenia
      oldStanPin = LOW;                           //Aktualizacja stanu przycisku
    }
    timeRoznica = timeStop - timeStart;           //Obliczenie różnicy czasu
    timeStop = 0;                                 //Wyzerowanie czasów
    timeStart = 0;                                //Wyzerowanie czasów
    if (timeRoznica > 100)                        //Sprawdza czy to nie drgania
    {
      poziomMenu++;                               //Jeśli nie to zwiększa poziom menu
      if ( poziomMenu >= 5)                       //Maks. poziom menu to 3
      {
        poziomMenu = 0;                           //Wraca do początku - menu działa w pętli
      }
      timeRoznica = 0;                            //Zeruje różnicę czasu
    }
    //xSemaphoreGive( serial_mutex ); // Now free or "Give" the Serial Port for others. 
    vTaskDelay(1);                                //Inne zadania
  }
}
/*
----------------------------------------------------------------------------------------------------------------
********************************************Zadanie regulaotra**************************************************
----------------------------------------------------------------------------------------------------------------
*/
void TaskRegulator( void *pvParameters )
{
  (void) pvParameters;
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  float up = 0, ui = 0, ud = 0, alfa = 0.3;    //Zmienne poszczególnych członów
  float  roznica = 0, przyrost_ud = 0;    //Zmienne do części różniczkowej
  float suma = 0, sumaStara = 0;                         //Zmienne do częsci całkowej 
  float h = 0.1;            //Okres impulsowania
  float uchyb = 0, uchybStary = 0, udStare = 0;       //Stary uchyb i bieżący;
  float umax=2000;            //Zmienna do anty-windup
  parametr parametry;
  lcd_obiekt lcdObiekt;
  //PARAMETRY POCZĄTKOWE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  parametry.KP = 1;
  parametry.TD = 3;
  parametry.TI = 5;
  parametry.WZ = 70;
  // for (int i=0;i<10;i++)
  //   vTaskDelay(1);
  for (;;)
  {   
    if( xQueueReceive( regulatorQueue, &( parametry ),( TickType_t ) 1 ) == pdPASS )
    {
      udStare = 0;
      sumaStara = 0;
      uchyb = 0;
      przyrost_ud = 0;
      uchybStary = 0;
      roznica = 0;
      sygnalSterujacy = 0;
      wartoscBiezaca = 0;
    }
    xSemaphoreTake( serial_mutex, portMAX_DELAY );        //Semafor na zmienne regulaltora i obiektu
    //OBliczenia kazdej składowej
    uchyb=parametry.WZ - wartoscBiezaca;              
    roznica=(uchyb - uchybStary)/h;
    przyrost_ud=(-1/(alfa*parametry.TD))*udStare+(parametry.TD/(alfa*parametry.TD) )*roznica;
    suma=uchyb+sumaStara;
    //POszczególne parametry regulatora
    up=parametry.KP*uchyb;
    ui=(h/parametry.TI)*suma;
    ud=udStare+h*przyrost_ud;
    //Sygnał sterujący
    sygnalSterujacy = up+ui+ud;       
    //Stare wartości
    uchybStary = uchyb;
    sumaStara = suma;
    udStare = ud;                                  //Wypisuje wartość
    // //anty-windup
    if (sygnalSterujacy<=0) { sygnalSterujacy=0;  suma=uchyb;}
    if (sygnalSterujacy>umax) {sygnalSterujacy=umax; suma-=uchyb;}
    //Wyświetlanie parametrów na wyświetlaczu
    lcdObiekt.uchyb = uchyb;
    lcdObiekt.sygnalSterujacy = sygnalSterujacy;
    lcdObiekt.wartoscBiezaca = wartoscBiezaca;
    lcdObiekt.WZ = parametry.WZ;
    // tft.setCursor(50, 100);
    // tft.print("           ");
    // tft.setCursor(50, 100);
    // tft.print(uchyb, 2);
    // tft.setCursor(50, 150);
    // tft.print("           ");
    // tft.setCursor(50, 150);
    // tft.print(sygnalSterujacy, 2);
    // tft.setCursor(50, 200);
    // tft.print("           ");
    // tft.setCursor(50, 200);
    // tft.print(wartoscBiezaca, 2);
    xQueueSend(lcd_regulatorQueue, &lcdObiekt, portMAX_DELAY);   //Wysyła parametry do regulatora
    xSemaphoreGive( serial_mutex );
    //vTaskDelay(1);
    vTaskDelayUntil( &xLastWakeTime, 100/portTICK_PERIOD_MS);
  }
}
/*
----------------------------------------------------------------------------------------------------------------
********************************************Zadanie obiektu*****************************************************
----------------------------------------------------------------------------------------------------------------
*/
void TaskObiekt( void *pvParameters )
{
  (void) pvParameters;
  float h = 0.1;
  // float dt = 0.001;
  // float u0 = 10;
  // float x1 = 0, x10 = 0;
  // int licznik = round(h/dt);
  // float Y;
  // Transmitancja
  //          K
  // G(s)= --------
  //        1 + Ts
  // float K = 2;
  // float T = 3;
  float H1, H2, h1 = 0, h2 = 0;
  // Transmitancja
  //              K1 * K2
  // G(s)= -------------------------
  //        (1 + T1*s)*(1 + T2*s)
  float K1 = 1,K2 = 2;
  float T1 = 2, T2 = 1;
  //Parametry biorników
  float k11,k21;
  float A1 = 14,A2 = 100;            //Powierznia przekroju zbiornika 1 i 2 
  float sn1 = 12;         //Przekrój przepływowy zaworu 1
  float sn2 = 30;         //Przekrój przepływowy zaworu 2
  float hn1 = 30;         //Poziom wody w punkcie pracy
  float hn2 = 60;         //Poziom wody w punkcie pracy
  float g = 10;           //Przyśpieszenie ziemskie
  k11 = sn1*sqrt(g/(2*hn1));
  k21 = sn2*sqrt(g/(2*hn2));
  K1 = 1/k11;             //Wzmocnienie 1 zbiornika
  T1 = A1/k11;            //Stała czasowa 1 zbiornika
  K2= 1/k21;             //Wzmocnienie 2 zbiornika
  T2 = A2/k21;            //Stała czasowa 2 zbiornika
  Serial.println(K1);
   Serial.println(K2);
   Serial.println(T1);
   Serial.println(T2);
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    xSemaphoreTake( serial_mutex, portMAX_DELAY );
    // for (int i = 0; i<licznik; i++)
    // {
    //   // x1=x10+dt*sygnalSterujacy;
    //   // wartoscBiezaca=sygnalSterujacy+dt*(5*u0-6*sygnalSterujacy-2*x10);
    //   // x10 = x1;
    // }
    //Obiekt 1
    // H1 = exp(-h/T1)*h1+K1*(1-exp(-h/12))*sygnalSterujacy;
    // h1 = H1;
    // wartoscBiezaca = H1;
    //Obiekt 2
    H1 = exp(-h/T1)*h1+K1*(1-exp(-h/12))*sygnalSterujacy;
    H2 = exp(-h/T2)*h2+K2*(1-exp(-h/12))*H1;
    h1 = H1;
    h2 = H2;
    wartoscBiezaca = H2;
    // if (wartoscBiezaca <= 0) { wartoscBiezaca = 0;}
    // if (wartoscBiezaca >= 100) { wartoscBiezaca = 100;}
    xSemaphoreGive( serial_mutex );
    //vTaskDelay(1);
    vTaskDelayUntil( &xLastWakeTime, 100/portTICK_PERIOD_MS);
  }
}

void TaskLCD(void *pvParameters)
{
  (void) pvParameters;
  // TickType_t xLastWakeTime;
  // parametr parametry;
  lcd_obiekt lcdObiekt;
  int poziom = 0;
  for (;;)
  {
    // if( xQueueReceive( lcd_parametryKPQueue, &( parametry.KP ),( TickType_t ) 1 ) == pdPASS )
    // {
    //   wyswietlanie(40, 1, parametry.KP);                 //Wyświetlanie zmiennej s 
    // }
    // if( xQueueReceive( lcd_parametryTIQueue, &( parametry.TI ),( TickType_t ) 1 ) == pdPASS )
    // {
    //   wyswietlanie(200, 1, parametry.TI);                 //Wyświetlanie zmiennej
    // }
    // if( xQueueReceive( lcd_parametryTDQueue, &( parametry.TD),( TickType_t ) 1 ) == pdPASS )
    // {
    //  wyswietlanie(40, 19, parametry.TD); 
    // }
    // if( xQueueReceive( lcd_parametryWZQueue, &( parametry.WZ),( TickType_t ) 1 ) == pdPASS )
    // {
    //   wyswietlanie(200, 19, parametry.WZ);                 //Wyświetlanie zmiennej
    // }
    // if( xQueueReceive( lcd_parametryQueue, &( parametry),( TickType_t ) 1 ) == pdPASS )
    // {
    //   wyswietlanie(40, 1, parametry.KP);                 //Wyświetlanie zmiennej s 
    //   wyswietlanie(40, 19, parametry.TD); 
    //   wyswietlanie(200, 1, parametry.TI);                 //Wyświetlanie zmiennej
    //   wyswietlanie(200, 19, parametry.WZ);                 //Wyświetlanie zmiennej
    // }
    if( xQueueReceive( lcd_regulatorQueue, &( lcdObiekt ),( TickType_t ) 1 ) == pdPASS)
    {
      tft.setCursor(50, 100);
      tft.print("        ");
      tft.setCursor(50, 100);
      tft.print(lcdObiekt.uchyb, 2);
      tft.setCursor(50, 150);
      tft.print("        ");
      tft.setCursor(50, 150);
      tft.print(lcdObiekt.sygnalSterujacy, 2);
      tft.setCursor(50, 200);
      tft.print("        ");
      tft.setCursor(50, 200);
      tft.print(lcdObiekt.wartoscBiezaca, 2);
      tft.drawRect(170, 60, 130, 170, ILI9341_GREEN);     //Rysuje zbiornik
      // //Zbiornik wysoki na 168
      //  ____________
      // |            | 168
      // |            | 
      // |            |
      // |            |
      // |            |
      // |            |
      // |            |
      // |____________| 0
      poziom = abs(round((lcdObiekt.wartoscBiezaca/100)*168));
      tft.fillRect(171, 61, 128, 168-poziom, ILI9341_BLACK);
      tft.fillRect(171, 229-poziom, 128, poziom, ILI9341_BLUE);
      tft.drawLine(171, 229-round((lcdObiekt.WZ/100)*168), 299, 229-round((lcdObiekt.WZ/100)*168), ILI9341_RED);  //Czerni starą linię   
    }
    vTaskDelay(1);
  }
}
/*
----------------------------------------------------------------------------------------------------------------
********************************************Funkcja obsługująca wyswietlacz*************************************
----------------------------------------------------------------------------------------------------------------
*/
void wyswietlanie(int x, int y, float wartosc)
{
  tft.setCursor(x, y);                   //Ustawia kursor
  tft.print ("      ");                                    //Usuwa starą wartość
  tft.setCursor(x, y);                   //Ustawia kursor
  tft.print(wartosc, 2);                                   //Wypisuje wartość
  tft.drawLine(0, 20, tft.width(),20, ILI9341_BLACK);  //Czerni starą linię
  tft.drawLine(0, 38, tft.width(),38, ILI9341_BLACK);  //Czerni starą linię
  tft.drawLine(x, y+19, x+60, y+19, ILI9341_RED);  //Rysuje czerwoną linię pod zmienianą zmienną
  tft.setCursor(300,200);
  //vTaskDelay(10/portTICK_PERIOD_MS);
}
/*
----------------------------------------------------------------------------------------------------------------
*********************************************Funkcja obsługująca przerwanie*************************************
----------------------------------------------------------------------------------------------------------------
*/
void isr()
{
  int wartoscA = digitalRead(pinA);       //Odczyt wartości z Pinu A
  int wartoscB = digitalRead(pinB);       //Odczyt wartości z Pinu A
  if ((millis() - time) > 3)              //Jeśli czas większy od zadanej wartości to sprawdza
  {
    if (wartoscA == wartoscB)             //Czy enkoder zliczył
    { 
      pozycjaEnkodera++;                  //Zwiększ pozycję
    }
    else
    {
      pozycjaEnkodera--;                  //Zmniejsz pozycję
    }
  }
  time = millis();                        //Aktualizacja czasu
}