#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SPIFFS.h>
#include <sqlite3.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <iomanip>

using namespace std;


#define STX 0xAA
#define ETX 0x55
#define MAX_ID_BYTES 16

// Adjust these to pins that are actually free on your specific board -
// GPIO16/17 are the Serial2 defaults on most ESP32 dev boards, but
// are used internally for PSRAM on WROVER modules. Check your board.
#define TWN4_RX_PIN 4
#define TWN4_TX_PIN 5
#define TWN4_BAUD   19200


enum ParseState {
  WAIT_STX,
  READ_TAGTYPE,
  READ_BITCOUNT_LO,
  READ_BITCOUNT_HI,
  READ_BYTECOUNT,
  READ_ID,
  READ_CHECKSUM,
  READ_ETX
};

ParseState state = WAIT_STX;
uint8_t  tagType;
uint16_t idBitCount;
uint8_t  idByteCount;
uint8_t  idBytes[MAX_ID_BYTES];
uint8_t  idIndex;
uint8_t  runningChecksum;


GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(
  GxEPD2_213_Z98c(10, 9, 8, 7) // CS, DC, RST, BUSY
);

String currentIP = "";

#define FORMAT_SPIFFS_IF_FAILED true
#define MAX_FILE_NAME_LEN 100
#define MAX_STR_LEN 500

char db_file_name[MAX_FILE_NAME_LEN] = "\0";
sqlite3 *db = NULL;
int rc;


AsyncWebServer server(80);

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  //Handle WebSocket event
}


//----------------------------------------------------------------------------------------------------
//Epaper Setup and Update Functions

void ePaperSetup(){
  display.init(115200, true, 2, false); // Waveshare reset circuit timing

  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, 30);
    display.print("Ready to display!");
  } while (display.nextPage());

  display.hibernate();
}

void updateDisplay(const char* text) {
  display.init(115200, false); // false = don't re-run full reset, just wake it
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, 30);
    display.print(text);
  } while (display.nextPage());

  display.hibernate(); // put it back to sleep until next update
}

//----------------------------------------------------------------------------------------------------
//WiFi Manager Callback Functions


void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("WiFi mode: %d\n", WiFi.getMode());
  Serial.printf("Station count: %d\n", WiFi.softAPgetStationNum());
}





void IPhandling(){
  if (WiFi.localIP().toString() != currentIP) {
    //updateDisplay("IP Address: ".toString().c_str());
    updateDisplay(WiFi.localIP().toString().c_str());
    currentIP = WiFi.localIP().toString();
  }
}


//----------------------------------------------------------------------------------------------------
//SQLite3 Callback Functions



int myCallback(void *data, int argc, char **argv, char **colNames) {
  for (int i = 0; i < argc; i++) {
    Serial.printf("%s = %s\n", colNames[i], argv[i] ? argv[i] : "NULL");
  }
  return 0;
}

int rowCountCallback(void *data, int argc, char **argv, char **colNames) {
  int *count = (int*)data;
  *count = atoi(argv[0]);
  return 0; // always continue normally
}

char *errMsg = 0;

vector<vector<String>> dbArr;

int dbToArrHelper(void *data, int argc, char **argv, char **colNames){
  

  vector<String> row;
  for (int i = 0; i < argc; i++) {
    row.push_back(argv[i] ? argv[i] : "NULL");
  }
  dbArr.push_back(row);
  return 0;
}

void dbToArr(){
  dbArr.clear();
  rc = sqlite3_exec(db, "SELECT * FROM persons;", dbToArrHelper, NULL, &errMsg);
}


void printDb() {
  
  for (int i = 0; i < dbArr.size(); i++) {
    for (int j = 0; j < dbArr[i].size(); j++) {
      Serial.print(dbArr[i][j]);
      if (j < dbArr[i].size() - 1) {
        Serial.print(", ");
      }
    }
    Serial.println();
  }
}

//----------------------------------------------------------------------------------------------------


void printFrame() {
  Serial.print("TagType=");
  Serial.print(tagType);
  Serial.print("  IDBitCount=");
  Serial.print(idBitCount);
  Serial.print("  ID=");
  for (int i = 0; i < idByteCount; i++) {
    if (idBytes[i] < 0x10) Serial.print("0");
    Serial.print(idBytes[i], HEX);
  }
  Serial.println();
}

//----------------------------------------------------------------------------------------------------



WiFiManager wifiManager;

void setup() {


  Serial.begin(115200);  // USB, for debug output - open Serial Monitor at 115200
  Serial2.begin(TWN4_BAUD, SERIAL_8N1, TWN4_RX_PIN, TWN4_TX_PIN);
  Serial.println("Waiting for TWN4...");


  SPIFFS.begin(true); // For SPIFFS
  sqlite3_initialize();

  //wifiManager.resetSettings();

  wifiManager.setAPCallback(configModeCallback);

  //first parameter is name of access point, second is the password
  wifiManager.autoConnect("AP-NAME", "AP-PASSWORD");



  ePaperSetup();




   // --- Add this ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<h1>Hello from ESP32!</h1><p>Current IP: " + WiFi.localIP().toString() + "</p>");
  });
  server.begin();


  rc = sqlite3_open("/spiffs/myData.db", &db);
  if (rc != SQLITE_OK) {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db));
  } else {
    Serial.println("Database opened successfully");
  }


  //sqlite3_exec(db, "DELETE FROM my_table;", NULL, NULL, &errMsg);
  rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS persons (id INTEGER, name TEXT);", NULL, NULL, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("CREATE TABLE error: %s\n", errMsg);
    sqlite3_free(errMsg);
  }

  sqlite3_exec(db, "DELETE FROM persons;", NULL, NULL, &errMsg);

  
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (1, 'GT', 676);", NULL, NULL, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("CREATE TABLE error: %s\n", errMsg);
    sqlite3_free(errMsg);
  }
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (2, 'Franco', 222);", NULL, NULL, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("CREATE TABLE error: %s\n", errMsg);
    sqlite3_free(errMsg);
  }
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (3, 'Ikem', 555);", NULL, NULL, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("CREATE TABLE error: %s\n", errMsg);
    sqlite3_free(errMsg);
  }
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (4, 'Raghav', 777);", NULL, NULL, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("CREATE TABLE error: %s\n", errMsg);
    sqlite3_free(errMsg);
  }
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (5, 'Dominic', 888);", NULL, NULL, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("CREATE TABLE error: %s\n", errMsg);
    sqlite3_free(errMsg);
  }

 

  dbToArr();
  printDb();
}

char str[MAX_STR_LEN];
void loop() {

  IPhandling();



  while (Serial2.available()) {
    uint8_t b = Serial2.read();

    Serial.println(b);

    switch (state) {
      case WAIT_STX:
        if (b == STX) {
          runningChecksum = 0;
          state = READ_TAGTYPE;
        }
        break;

      case READ_TAGTYPE:
        tagType = b;
        runningChecksum ^= b;
        state = READ_BITCOUNT_LO;
        break;

      case READ_BITCOUNT_LO:
        idBitCount = b;
        runningChecksum ^= b;
        state = READ_BITCOUNT_HI;
        break;

      case READ_BITCOUNT_HI:
        idBitCount |= ((uint16_t)b << 8);
        runningChecksum ^= b;
        state = READ_BYTECOUNT;
        break;

      case READ_BYTECOUNT:
        idByteCount = b;
        runningChecksum ^= b;
        idIndex = 0;
        state = (idByteCount > 0 && idByteCount <= MAX_ID_BYTES) ? READ_ID : WAIT_STX;
        break;

      case READ_ID:
        idBytes[idIndex++] = b;
        runningChecksum ^= b;
        if (idIndex >= idByteCount) {
          state = READ_CHECKSUM;
        }
        break;

      case READ_CHECKSUM:
        if (b == runningChecksum) {
          state = READ_ETX;
        } else {
          Serial.println("Checksum mismatch, dropping frame");
          state = WAIT_STX;
        }
        break;

      case READ_ETX:
        if (b == ETX) {
          printFrame();
        } else {
          Serial.println("Missing ETX, dropping frame");
        }
        state = WAIT_STX;
        break;
    }
  }

}