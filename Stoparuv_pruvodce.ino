#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <GxEPD2_3C.h>

// Nová knihovna pro ultimátní češtinu!
#include <U8g2_for_Adafruit_GFX.h>

// --- NASTAVENÍ WI-FI ---
const char* ssid = "xxx";
const char* password = "xxx";

// --- PINY ---
#define EPD_CS   15
#define EPD_DC   27
#define EPD_RST  26
#define EPD_BUSY 25
#define EPD_SCK  18
#define EPD_MISO 19
#define EPD_MOSI 23
#define EPD_PWR  4

#define BTN_HOME    32  
#define BTN_RANDOM  33  
#define BTN_SEARCH  14  
#define BTN_UP      22  
#define BTN_DWN     21  
#define BTN_LEFT    16  
#define BTN_RIGHT   17  

GxEPD2_3C<GxEPD2_420c_GDEY042Z98, GxEPD2_420c_GDEY042Z98::HEIGHT> display(GxEPD2_420c_GDEY042Z98(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

const String randomUrl = "https://cs.wikipedia.org/w/api.php?action=query&generator=random&grnnamespace=0&prop=extracts&exintro&explaintext&format=json&origin=*";

String currentTitle = "";
String currentText = "";
int currentPage = 0;
const int CHARS_PER_PAGE = 450; 
bool isReadingArticle = false;

// =========================================================================
// SEM VLOŽ SVŮJ VYGENEROVANÝ KÓD Z IMAGE2CPP (Následující 3 řádky nahraď)
// =========================================================================
const unsigned char logo_dont_panic [] PROGMEM = {
  0xff, 0xff, 0xff // Zástupný kód - NAHRADIT!
};
// =========================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

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

  // Inicializace české U8G2 knihovny a propojení s displejem
  u8g2Fonts.begin(display);
  u8g2Fonts.setFontMode(1);                 // Transparentní pozadí písma
  u8g2Fonts.setFontDirection(0);            // Psaní zleva doprava
  
  connectWiFi();
  drawHomeScreen();
}

void loop() {
  if (digitalRead(BTN_HOME) == HIGH || digitalRead(BTN_LEFT) == HIGH) {
    drawHomeScreen();
    delay(1000); 
  }
  
  if (digitalRead(BTN_RANDOM) == HIGH || digitalRead(BTN_RIGHT) == HIGH) {
    fetchAndDrawRandomArticle();
    delay(1000);
  }
  
  if (digitalRead(BTN_SEARCH) == HIGH) {
    drawErrorScreen("Babel fish offline", "Hlasový modul nenalezen. Zapojte I2S mikrofon do portu Sub-Etha nebo zkuste nepravděpodobnostní pohon.");
    isReadingArticle = false;
    delay(1000);
  }

  if (digitalRead(BTN_DWN) == HIGH && isReadingArticle) {
    int maxPages = (currentText.length() / CHARS_PER_PAGE);
    if (currentPage < maxPages) {
      currentPage++;
      drawCurrentArticlePage();
    }
    delay(1000);
  }

  if (digitalRead(BTN_UP) == HIGH && isReadingArticle) {
    if (currentPage > 0) {
      currentPage--;
      drawCurrentArticlePage();
    }
    delay(1000);
  }
  
  delay(50);
}

void connectWiFi() {
  Serial.print("Pripojovani k siti...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void fetchAndDrawRandomArticle() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  HTTPClient http;
  http.begin(randomUrl);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    drawErrorScreen("Chyba spojení", "Spojení s Galaktickou databází selhalo.");
    isReadingArticle = false;
    return;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    drawErrorScreen("Chyba dat", "Data byla zničena v hyperprostoru.");
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
     currentText = "Heslo sice existuje, ale neobsahuje žádný text. Zřejmě ho snědla blýskavice.";
  }

  currentPage = 0;
  isReadingArticle = true;
  drawCurrentArticlePage();
}

// Geniální funkce pro zalamování dlouhého textu s diakritikou
void printWrappedText(String text, int startX, int startY, int maxWidth) {
  int x = startX;
  int y = startY;
  int lineHeight = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent() + 4; // Výška řádku
  
  String word = "";
  for (int i = 0; i <= text.length(); i++) {
    char c = (i < text.length()) ? text.charAt(i) : ' ';
    if (c == ' ' || c == '\n' || i == text.length()) {
      if (word.length() > 0) {
        int wordWidth = u8g2Fonts.getUTF8Width(word.c_str());
        if (x + wordWidth > startX + maxWidth) {
          x = startX;
          y += lineHeight;
        }
        u8g2Fonts.setCursor(x, y);
        u8g2Fonts.print(word);
        x += wordWidth + u8g2Fonts.getUTF8Width(" ");
        word = "";
      }
      if (c == '\n') {
        x = startX;
        y += lineHeight;
      }
    } else {
      word += c;
    }
  }
}

void drawCurrentArticlePage() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Černá lišta
    display.fillRect(0, 0, 300, 25, GxEPD_BLACK);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf); 
    
    u8g2Fonts.setCursor(10, 18);
    u8g2Fonts.print("Mk.III");
    
    int maxPages = (currentText.length() / CHARS_PER_PAGE) + 1;
    u8g2Fonts.setCursor(220, 18);
    u8g2Fonts.print("STR " + String(currentPage + 1) + "/" + String(maxPages));

    // Nadpis červeně
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf); 
    
    // Zalamování nadpisu
    printWrappedText(currentTitle, 10, 50, 280);

    display.fillRect(10, 75, 280, 3, GxEPD_RED);

    // Text černě
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf); // Velikost pro čtení
    
    int startIndex = currentPage * CHARS_PER_PAGE;
    int endIndex = startIndex + CHARS_PER_PAGE;
    if (endIndex > currentText.length()) endIndex = currentText.length();
    
    String pageText = currentText.substring(startIndex, endIndex);
    if (endIndex < currentText.length()) pageText += "..."; 
    
    // Zalamování článku
    printWrappedText(pageText, 10, 105, 280);
    
  } while (display.nextPage());
}

void drawErrorScreen(String title, String message) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0, 0, 300, 25, GxEPD_BLACK);
    
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);
    u8g2Fonts.setCursor(10, 18);
    u8g2Fonts.print("CHYBA SYSTÉMU");

    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setCursor(10, 90);
    u8g2Fonts.print(title);

    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);
    printWrappedText(message, 10, 140, 280);

  } while (display.nextPage());
}

void drawHomeScreen() {
  isReadingArticle = false;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    display.fillRect(0, 0, 300, 25, GxEPD_BLACK);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);
    
    u8g2Fonts.setCursor(10, 18);
    u8g2Fonts.print("Mk.III");
    u8g2Fonts.setCursor(200, 18);
    u8g2Fonts.print("WIFI 42%");

    // Vykreslení nahraného Loga (Vycentrované horizontálně)
    // 300 šířka displeje - 200 šířka loga / 2 = 50px X pozice
    // Výška loga je 200px, začíná na Y = 35 -> končí na 235
    display.drawBitmap(50, 35, logo_dont_panic, 200, 200, GxEPD_BLACK);
    
    // Oddělovač
    display.fillRect(30, 250, 240, 4, GxEPD_RED); 
    
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);
    
    // Nyní už v naprosto čisté češtině!
    u8g2Fonts.setCursor(35, 290);
    u8g2Fonts.print("SET / DOPRAVA: Náhoda");
    
    u8g2Fonts.setCursor(35, 330);
    u8g2Fonts.print("MID: Hledat (Mikrofon)");
    
    u8g2Fonts.setCursor(35, 370);
    u8g2Fonts.print("NAHORU / DOLŮ: Čtení textu");

  } while (display.nextPage());
}
