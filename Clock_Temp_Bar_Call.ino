/* Sketch voor datum, UTC tijd, Lokale tijd, temperatuur, lucht vochtigheid en luchtdruk
   in de shack te tonen op een I2C LCD display
   Er wordt hierbij gebruik gemaakt van een DS1307 RTC Clock module, een DHT11 Temperatuursensor
   en een BMP280 luchtdruk sensor. Als alternatief kan ook een DS3231 RTC gebruikt worden

   *************************************************************************************************
   *** De Clock module ***
   Initialisatie van de datum en tijd op basis van UTC in de DS1307 RTC module
   Dit moet voorafgaande aan deze sketch 1x uitgevoerd worden om de datum en tijd in de clock module
   goed te zetten op UTC tijd
   Dit kan aan de hand van het voorbeeld wat in de Library van de DS1307 staat

   ************************************** 
   * Zet vooraf wel even de PC op UTC ! *
   * ************************************

   Instellen datum en tijd (UTC) in de Clock module (methode 1)
   Ga hier voor naar de library DS1307RTC en ga naar Examples
   Kies hier in de map SetTime de sketch SetTime.ino, zet de Seriele monitor aan en run deze sketch.  
   Als alles goed doorlopen is, is nu de clock module geinitialiseerd en voorzien van de juiste
   datum en tijd. 
   
   Instellen datum en tijd (UTC) in de Clock module (methode 2)
   Gebruik hiervoor de aparte sketch "RTC Data and Time setter" welke op Internet
   te vinden is. (WWW Sketch) Dan hoef je niet je PC eerst op UTC te zetten.
    
   *** Barometer module ***
   Toegepast de BMP280
   Deze kan eventueel ook nog gebruikt worden om hoogte aan te geven
   ***********************************************************************************************
      
   Oorspronkelijke (beperkte) sketch van Timofte Andrei vertaald, aangepast, uitgebreid door PA0GTB
   en verder gestructureerd door Edwin, PA7FRN
   Version 1.8.7 changed by PA7FRN and first commit in git repository.
   Version 1.8.8 Maart 2017
   Version 1.8.9 Maart 2017
   
*/
   
// Benoem de benodigde Libraries

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <dht11.h>
#include <Time.h>
#include <Timezone.h>    
#include <DS1307RTC.h> 
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// Declareer de constanten en Pin nummers

#define DHT11PIN 2 // dht11 signal pin connected to D2

#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11 
#define BMP_CS 10

#define LCD_ROW_COUNT  4
#define LCD_COL_COUNT 20

#define LCD_ROW_DATE 0
#define LCD_ROW_CALL 0
#define LCD_ROW_UTC  1
#define LCD_ROW_TIME 1
#define WETHER_ROW   2
#define PRESSURE_ROW 3

#define LCD_COL_CALL 1
#define LCD_COL_DATE 9

#define LCD_COL_UTC   0
#define LCD_COL_TIME 10

#define THERM_SYMBOL_COL     1
#define THERM_VALUE_COL      3
#define GRAD_SYMBOL_COL      5
#define HUMIDITY_SYMBOL_COL 10
#define HUMIDITY_VAL_COL    12
#define PROC_SYMBOL_COL     14

#define PRESSURE_SYMBOL_COL 3
#define PRESSURE_VAL_COL    8

#define THERM_SYMBOL    1
#define HUMIDITY_SYMBOL 2
#define PRESSURE_SYMBOL 3

#define DATE_FORMAT_dd_mm_yyyy 0
#define DATE_FORMAT_ddMMMyyyy  1

// ----------- User preferences -------------
#define CALLSIGN "PA0GTB" //Put your callsign here

//Choose ad date format (DATE_FORMAT_dd_mm_yyyy or DATE_FORMAT_ddMMMyyyy)
//static int dateFormat = DATE_FORMAT_dd_mm_yyyy;
static int dateFormat = DATE_FORMAT_ddMMMyyyy;

// change these strings if you want another lanuage
String strMonth[12] = {
	"JAN", "FEB", "MAR",  "APR",  "MEI",  "JUN",  "JUL",  "AUG",  "SEP",  "OKT",  "NOV",  "DEC"
};
// ----------- End User preferences ---------

// Afwijkende sybolen maken

byte thermometer[8] = //symbool voor thermometer
{
    B00100,
    B01010,
    B01010,
    B01110,
    B01110,
    B11111,
    B11111,
    B01110
};

byte humiditySymbol[8] = //symbool voor waterdruppel 
{
    B00100,
    B00100,
    B01010,
    B01010,
    B10001,
    B10001,
    B10001,
    B01110,
};

byte pressure[8] = //symbool voor Luchtdruk 
{
    B11111,
    B10001,
    B10001,
    B10001,
    B11110,
    B10000,
    B10000,
    B10000
};

// Declareer de verschillende objecten

// Temperatuur en vochtigheid Sensor
dht11 DHT11;

//Central European Time (Amsterdam, Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);

TimeChangeRule *tcr;        //pointer to the time change rule, use to get the TZ abbrev
time_t utc;

/* Display
   set the LCD address to 0x27 for a 20 chars 4 line display
   Set the pins on the I2C chip used for LCD connections: 
*/

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  

Adafruit_BMP280 bme; // I2C bus

void setup()   // SETUP: Wordt eenmaal doorlopen
{
  // Initialisatie display en gebruikte symbolen
  
  Wire.begin();
  lcd.begin(LCD_COL_COUNT, LCD_ROW_COUNT);
  lcd.backlight();
  lcd.clear();
  lcd.createChar(THERM_SYMBOL,thermometer);
  lcd.createChar(HUMIDITY_SYMBOL,humiditySymbol);
  lcd.createChar(PRESSURE_SYMBOL,pressure);

  // Get time from RTC

  Wire.beginTransmission(0x68);
  Wire.write(0x07); // move pointer to SQW address
  Wire.write(0x10); // sends 0x10 (hex) 00010000 (binary) to control register - turns on square wave
  Wire.endTransmission();

  setSyncProvider(RTC.get);  

  lcd.setCursor(LCD_COL_CALL, LCD_ROW_CALL);
  lcd.print(CALLSIGN);

  lcd.setCursor(THERM_SYMBOL_COL, WETHER_ROW);
  lcd.write(THERM_SYMBOL);

  lcd.setCursor(GRAD_SYMBOL_COL, WETHER_ROW);
  lcd.print((char)223); //graden symbool
  lcd.print("C");

  lcd.setCursor(HUMIDITY_SYMBOL_COL, WETHER_ROW);
  lcd.write(HUMIDITY_SYMBOL);

  lcd.setCursor(PROC_SYMBOL_COL, WETHER_ROW);
  lcd.print("%");

  lcd.setCursor(PRESSURE_SYMBOL_COL, PRESSURE_ROW);
  lcd.write(PRESSURE_SYMBOL);
  lcd.print(F(" ="));
}/*--(end setup )---*/

void loop()   /*----( LOOP: RUNS CONSTANT )----*/
{
  utc = now();
  time_t t = CE.toLocal(utc, &tcr);
  DHT11.read(DHT11PIN);
    
  printTime(utc, LCD_COL_UTC , LCD_ROW_UTC );
  printTime(t  , LCD_COL_TIME, LCD_ROW_TIME);
  printDate(t  , LCD_COL_DATE, LCD_ROW_DATE);
  toon_weather();       //displaying temperatuur, vochtigheid en luchtdruk
}

void toon_weather() {
  // Toon temperatuur, vochtigheid en luchtdruk op het display
  sPrintRightAlign(DHT11.temperature, 2, THERM_VALUE_COL , WETHER_ROW);
  sPrintRightAlign(DHT11.humidity   , 2, HUMIDITY_VAL_COL, WETHER_ROW);
  
  // barometer
//sPrintRightAlign(round(bme.readPressure()/100), 4, PRESSURE_VAL_COL, PRESSURE_ROW); // vrijzetten als sensor er straks aan hangt
  sPrintRightAlign(round(101500            /100), 4, PRESSURE_VAL_COL, PRESSURE_ROW); // Deze regel is nu alleen voor test op display
  lcd.print(" mBar");
  
  Serial.println();
  delay(1000);  // refresh elke sec  
}

void printTime(time_t t, int col, int row) {
  lcd.setCursor(col, row);
  sPrintDigits(hour(t));
  lcd.print(":");
  sPrintDigits(minute(t));
  lcd.print(":");
  sPrintDigits(second(t));
}

void printDate(time_t t, int col, int row) {
  lcd.setCursor(col, row);
  switch (dateFormat) {
    case DATE_FORMAT_dd_mm_yyyy:
      sPrintDigits(day(t));
      lcd.print("-");
      sPrintDigits(month(t));
      lcd.print("-");
      lcd.print(String(year(t)));
	  break;
	case DATE_FORMAT_ddMMMyyyy:
      sPrintDigits(day(t));
      lcd.print(strMonth[month(t)]);
      lcd.print(String(year(t)));
  }
}

void sPrintDigits(int val) {
  if(val < 10) {
    lcd.print("0");
  }
  lcd.print(String(val));
}

void sPrintRightAlign(int val, int positions, int col, int row) {
  lcd.setCursor(col, row);
  String strVal = String(val);
  while (strVal.length() < positions) {
    strVal = " " + strVal;
  }
  lcd.print(strVal);
}

/* ( THE END ) */

