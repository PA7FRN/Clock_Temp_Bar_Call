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
   Version 1.8.9 Maart 2017 Date format
   Version 1.9.0 Maart 2017 Task management
   Version 1.9.1 Maart 2017 Command line interface for setting callsign and date format
   Version 1.9.2 Interface for setting time via serial port
   Version 1.9.3 Write and read peferences to and from EEPROM
   Version 1.9.4 Use other temperature/hunidity sensor. Add decimal to temperature and hunidity value. Show version on LCD
   Version 1.9.5 Show welcome text with version number and list off commands at startup / start of serial connection. Version removed from display.
*/
   
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AM2320.h>  // nieuwe sensor
#include <Time.h>
#include <Timezone.h>    
#include <DS1307RTC.h> 
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <EEPROM.h>

#define VERSION_STRING "V1.9.5"

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
#define VERSION_ROW  3
#define PRESSURE_ROW 3

//positions in LCD row 0
#define LCD_COL_CALL   0
#define LCD_EMPTY_CALL "          "
#define LCD_COL_DATE   10
#define LCD_EMPTY_DATE "          "

//positions in LCD row 1
#define LCD_COL_UTC   0
#define LCD_COL_TIME 10

//positions in LCD row 2
#define THERM_SYMBOL_COL     0
#define THERM_VALUE_COL      2
#define GRAD_SYMBOL_COL      6
#define HUMIDITY_SYMBOL_COL 10
#define HUMIDITY_VAL_COL    12
#define PROC_SYMBOL_COL     16

//positions in LCD row 3
#define PRESSURE_SYMBOL_COL  4
#define PRESSURE_VAL_COL     7
#define MBAR_COL            12

#define THERM_SYMBOL    1
#define HUMIDITY_SYMBOL 2
#define PRESSURE_SYMBOL 3

#define DATE_FORMAT_dd_mm_yyyy 0
#define DATE_FORMAT_ddMMMyyyy  1
#define DATE_FROMAT_MAX        1

#define CDM_TIME    "T"
#define CMD_CALL    "call"
#define CMD_DF      "df"
#define CMD_VERSION "ver"

#define ADDR_CALL        0
#define ADDR_DATE_FORMAT 10


int dateFormat = DATE_FORMAT_dd_mm_yyyy;

// change these strings if you want another lanuage
String strMonth[12] = {
	"JAN", "FEB", "MRT",  "APR",  "MEI",  "JUN",  "JUL",  "AUG",  "SEP",  "OKT",  "NOV",  "DEC"
};

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
AM2320 th;

//Central European Time (Amsterdam, Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);

TimeChangeRule *tcr;        //pointer to the time change rule, use to get the TZ abbrev
time_t utc;

const unsigned long taskTime = 2000;
unsigned long taskTimer = 0;
String callsign = "<callsign>";

/* Display
   set the LCD address to 0x27 for a 20 chars 4 line display
   Set the pins on the I2C chip used for LCD connections: 
*/

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  

Adafruit_BMP280 bme; // I2C bus

void setup() {
  Serial.begin(9600); 
  Wire.begin();

  // Initialisatie display en gebruikte symbolen
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

  callsign = stringFromEeprom(ADDR_CALL);
  dateFormat = EEPROM.read(ADDR_DATE_FORMAT);
  if (dateFormat > DATE_FROMAT_MAX) {
    dateFormat = 0;
  }

  lcd.setCursor(LCD_COL_CALL, LCD_ROW_CALL);
  lcd.print(callsign);

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
  lcd.print("=");
  lcd.setCursor(MBAR_COL, PRESSURE_ROW);
  lcd.print("mBar");

  Serial.println("Clock_Temp_Bar_Call " + String(VERSION_STRING));
  Serial.println("Settings can be made via this serial interface.");
  handleCommand("", "");
}

void loop() {
  utc = now();
  time_t t = CE.toLocal(utc, &tcr);

  printTime(utc, LCD_COL_UTC , LCD_ROW_UTC );
  printTime(t  , LCD_COL_TIME, LCD_ROW_TIME);
  printDate(t  , LCD_COL_DATE, LCD_ROW_DATE);

  checkSerialInput();
  
  unsigned long currentMillis = millis();
  if (currentMillis - taskTimer >= taskTime) {
    taskTimer = currentMillis;
    toon_weather();       //displaying temperatuur, vochtigheid en luchtdruk
  }
}

void checkSerialInput() {
  String input = Serial.readString();
  if (input != "") {
    Serial.println();
    Serial.println(input);
    input.trim();
    int idx = input.indexOf(" ");
    String cmd = input;
    String par = "";
    if (idx > -1) {
      cmd = input.substring(0, idx);
      par = input.substring(idx+1);
    }
    cmd.trim();
    par.trim();
    handleCommand(cmd, par);
  }
}

void handleCommand(String cmd, String par) {
  if (cmd == CMD_CALL) {
    if ((par == "?") || (par == "")) {
      Serial.print("type ");
      Serial.print(CMD_CALL);
      Serial.println(" folowed by callsign, like:");
      Serial.print(CMD_CALL);
      Serial.println(" PA7FRN");
    }
    else {
      callsign = par;
      lcd.setCursor(LCD_COL_CALL, LCD_ROW_CALL);
      lcd.print(LCD_EMPTY_CALL);
      lcd.setCursor(LCD_COL_CALL, LCD_ROW_CALL);
      lcd.print(callsign);
      stringToEeprom(ADDR_CALL, callsign);
      Serial.print(callsign);
      Serial.println(" set");
    }
  }
  else if (cmd == CMD_DF) {
    if (par == "dd_mm_yyyy") {
      lcd.setCursor(LCD_COL_DATE, LCD_ROW_DATE);
      lcd.print(LCD_EMPTY_DATE);
      dateFormat = DATE_FORMAT_dd_mm_yyyy;
      EEPROM.write(ADDR_DATE_FORMAT, dateFormat);
      Serial.print(par);
      Serial.println(" set");
    }
    else if (par == "ddMMMyyyy") {
      lcd.setCursor(LCD_COL_DATE, LCD_ROW_DATE);
      lcd.print(LCD_EMPTY_DATE);
      dateFormat = DATE_FORMAT_ddMMMyyyy;
      EEPROM.write(ADDR_DATE_FORMAT, dateFormat);
      Serial.print(par);
      Serial.println(" set");
    }
    else {
      Serial.print("type ");
      Serial.print(CMD_DF);
      Serial.println(" folowed by format. Valid formats are:");
      Serial.print(CMD_DF);
      Serial.println(" dd_mm_yyyy");
      Serial.print(CMD_DF);
      Serial.println(" ddMMMyyyy");
    }
  }
  else if (cmd == CDM_TIME) {
    unsigned long pctime;
    const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013
 
    pctime = par.toInt();
    if( pctime >= DEFAULT_TIME) {
      setTime(pctime); 
      Serial.println("time set");
    }
    else {
      Serial.println("give the amount of seconds from 00:00 01-01-1970");
    }
  }
  else if (cmd ==  CMD_VERSION) {
    Serial.println(VERSION_STRING);
  }
  else {
    Serial.println("Valid commands are:");
    Serial.print("  ");
    Serial.print(CMD_CALL);
    Serial.println(" to set the callsign"   );
    Serial.print("  ");
    Serial.print(CMD_DF);
    Serial.println(" to set the date format");
    Serial.print("  ");
    Serial.print(CDM_TIME);
    Serial.println(" to set date and time"  );
    Serial.print("  ");
    Serial.print(CMD_VERSION);
    Serial.println(" gives the version number"  );
  }
}

void toon_weather() {
  th.Read();

  // Toon temperatuur, vochtigheid en luchtdruk op het display
  sPrintRightAlign(th.t, 4, 1, THERM_VALUE_COL , WETHER_ROW);
  sPrintRightAlign(th.h, 4, 1, HUMIDITY_VAL_COL, WETHER_ROW);
  
  // barometer
//sPrintRightAlign((bme.readPressure()/100), 4, 0, PRESSURE_VAL_COL, PRESSURE_ROW); // vrijzetten als sensor er straks aan hangt
  sPrintRightAlign((101500            /100), 4, 0, PRESSURE_VAL_COL, PRESSURE_ROW); // Deze regel is nu alleen voor test op display
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
      lcd.print(strMonth[month(t)-1]);
      lcd.print(String(year(t)));
      break;
  }
}

void sPrintDigits(int val) {
  if(val < 10) {
    lcd.print("0");
  }
  lcd.print(String(val));
}

void sPrintRightAlign(float val, int positions, int decimals, int col, int row) {
  lcd.setCursor(col, row);
  String strVal = String(val, decimals);
  while (strVal.length() < positions) {
    strVal = " " + strVal;
  }
  lcd.print(strVal);
}

void stringToEeprom(int eepromAddress, String aString) {
  int stringLength = aString.length();
  EEPROM.write(eepromAddress, stringLength);
  for (int i=0; i<stringLength; i++) {
    EEPROM.write(eepromAddress+1+i, aString[i]);
  }
}

String stringFromEeprom(int eepromAddress) {
  String resultString = "";
  int stringLength = EEPROM.read(eepromAddress);
  for (int i=0; i<stringLength; i++) {
    resultString = resultString + " ";
    resultString[i] = EEPROM.read(eepromAddress+1+i);
  }
  return resultString;
}
  
/* ( THE END ) */

