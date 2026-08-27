#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <GxEPD2_3C.h>

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// --- NASTAVENÍ WI-FI ---
const char* ssid = "Internet";
const char* password = "1234567890";

// --- PINY DISPLEJE ---
#define EPD_CS   15
#define EPD_DC   27
#define EPD_RST  26
#define EPD_BUSY 25
#define EPD_SCK  18
#define EPD_MISO 19
#define EPD_MOSI 23
#define EPD_PWR  4

// --- PINY JOYSTICKU A TLAČÍTEK ---
#define BTN_HOME    32  // SET
#define BTN_RANDOM  33  // RST
#define BTN_SEARCH  14  // MID
#define BTN_UP      22  // UP
#define BTN_DWN     21  // DWN
#define BTN_LEFT    16  // LET
#define BTN_RIGHT   17  // RHT

// Profil displeje
GxEPD2_3C<GxEPD2_420c_GDEY042Z98, GxEPD2_420c_GDEY042Z98::HEIGHT> display(GxEPD2_420c_GDEY042Z98(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

const String randomUrl = "https://cs.wikipedia.org/w/api.php?action=query&generator=random&grnnamespace=0&prop=extracts&exintro&explaintext&format=json&origin=*";

// --- STAV APLIKACE ---
String currentTitle = "";
String currentText = "";
int currentPage = 0;
const int CHARS_PER_PAGE = 450; 
bool isReadingArticle = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  // --- ZMĚNA ZDE: POUŽÍVÁME PULL-DOWN ---
  // Tlačítka jsou v klidu na LOW, při stisku (spojení s 3V3 přes COM) jdou na HIGH
  pinMode(BTN_HOME, INPUT_PULLDOWN);
  pinMode(BTN_RANDOM, INPUT_PULLDOWN);
  pinMode(BTN_SEARCH, INPUT_PULLDOWN);
  pinMode(BTN_UP, INPUT_PULLDOWN);
  pinMode(BTN_DWN, INPUT_PULLDOWN);
  pinMode(BTN_LEFT, INPUT_PULLDOWN);
  pinMode(BTN_RIGHT, INPUT_PULLDOWN);

  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);
  delay(100); 

  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS); 
  display.init(115200, true, 50, false); 
  display.setRotation(1); 

  connectWiFi();
  drawHomeScreen();
}

void loop() {
  // --- ZMĚNA ZDE: REAGUJEME NA HIGH ---
  
  if (digitalRead(BTN_HOME) == HIGH || digitalRead(BTN_LEFT) == HIGH) {
    Serial.println("Akce: DOMU");
    drawHomeScreen();
    delay(1000); 
  }
  
  if (digitalRead(BTN_RANDOM) == HIGH || digitalRead(BTN_RIGHT) == HIGH) {
    Serial.println("Akce: NAHODA");
    fetchAndDrawRandomArticle();
    delay(1000);
  }
  
  if (digitalRead(BTN_SEARCH) == HIGH) {
    Serial.println("Akce: HLEDAT");
    drawErrorScreen("Babel fish offline", "Hlasovy modul nenalezen. Zapojte I2S mikrofon do portu Sub-Etha nebo zkuste nepravdepodobnostni pohon (tlacitko doprava).");
    isReadingArticle = false;
    delay(1000);
  }

  if (digitalRead(BTN_DWN) == HIGH && isReadingArticle) {
    int maxPages = (currentText.length() / CHARS_PER_PAGE);
    if (currentPage < maxPages) {
      Serial.println("Akce: DALSI STRANA");
      currentPage++;
      drawCurrentArticlePage();
    }
    delay(1000);
  }

  if (digitalRead(BTN_UP) == HIGH && isReadingArticle) {
    if (currentPage > 0) {
      Serial.println("Akce: PREDCHOZI STRANA");
      currentPage--;
      drawCurrentArticlePage();
    }
    delay(1000);
  }
  
  delay(50);
}

void connectWiFi() {
  Serial.print("Pripojovani k siti Sub-Etha (Wi-Fi)");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nPripojeno!");
}

void fetchAndDrawRandomArticle() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  
  HTTPClient http;
  http.begin(randomUrl);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    drawErrorScreen("Chyba spojeni", "Spojeni s Galaktickou databazi selhalo.");
    isReadingArticle = false;
    return;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    drawErrorScreen("Chyba dat", "Data znicena v hyperprostoru.");
    isReadingArticle = false;
    return;
  }

  JsonObject pages = doc["query"]["pages"];
  currentTitle = "";
  currentText = "";

  for (JsonPair kv : pages) {
    currentTitle = kv.value()["title"].as<String>();
    currentText = kv.value()["extract"].as<String>();
    break; 
  }

  if (currentText.length() == 0) {
     currentText = "Heslo sice existuje, ale neobsahuje zadny text. Zrejme ho snedla blyskavice.";
  }

  currentPage = 0;
  isReadingArticle = true;
  drawCurrentArticlePage();
}

void drawCurrentArticlePage() {
  Serial.println("Prekresluji stranu " + String(currentPage + 1));
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    display.fillRect(0, 0, 300, 25, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(10, 17);
    display.print("Mk.III");
    
    display.setCursor(200, 17);
    int maxPages = (currentText.length() / CHARS_PER_PAGE) + 1;
    display.print("STR " + String(currentPage + 1) + "/" + String(maxPages));

    display.setTextColor(GxEPD_RED); 
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(10, 60);
    display.print(currentTitle);

    display.fillRect(10, 75, 280, 3, GxEPD_RED);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(10, 105);
    
    int startIndex = currentPage * CHARS_PER_PAGE;
    int endIndex = startIndex + CHARS_PER_PAGE;
    if (endIndex > currentText.length()) {
      endIndex = currentText.length();
    }
    
    String pageText = currentText.substring(startIndex, endIndex);
    if (endIndex < currentText.length()) {
      pageText += "..."; 
    }
    
    display.print(pageText);
    
  } while (display.nextPage());
}

void drawErrorScreen(String title, String message) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0, 0, 300, 25, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(10, 17);
    display.print("CHYBA SYSTEMU");

    display.setTextColor(GxEPD_RED);
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(10, 100);
    display.print(title);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(10, 150);
    display.print(message);
  } while (display.nextPage());
}

void drawHomeScreen() {
  Serial.println("Vykresluji hlavni obrazovku...");
  isReadingArticle = false;
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    display.fillRect(0, 0, 300, 25, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(10, 17);
    display.print("Mk.III");
    display.setCursor(200, 17);
    display.print("WIFI 42%");

    for(int i = 0; i < 4; i++) {
      display.drawCircle(150, 100, 40 + i, GxEPD_BLACK);
    }
    display.setTextColor(GxEPD_RED);
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(124, 114);
    display.print("42"); 
    
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold18pt7b); 
    display.setCursor(45, 195);
    display.print("STOPARUV");
    display.setCursor(45, 235);
    display.print("PRUVODCE");

    display.fillRect(30, 265, 240, 4, GxEPD_RED); 
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(25, 305);
    display.print("RST / DOPRAVA: Nahoda");
    display.setCursor(25, 335);
    display.print("MID: Hledat (Mikrofon)");
    display.setCursor(25, 365);
    display.print("NAHORU / DOLU: Cteni");

  } while (display.nextPage());
}