#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SPIFFS.h>
#include <sqlite3.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <iomanip>
#include <sstream>

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

#define MAX_BUFFER 128

#define DEBUG 1


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

// 'pixil-frame-0(1)', 50x50px
const unsigned char epd_bitmap_telaeris_sprite [] PROGMEM = {
	0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x03, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x9c, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x87, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x7f, 0x83, 0x80, 0x00, 0x00, 0x00, 0x00, 0xff, 0x81, 0xc0, 0x00, 0x00, 0x00, 
	0x01, 0xff, 0x80, 0xe0, 0x00, 0x00, 0x00, 0x03, 0xff, 0x88, 0x70, 0x00, 0x00, 0x00, 0x07, 0xff, 
	0x8c, 0x38, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x8e, 0x1c, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x87, 0x0e, 
	0x00, 0x00, 0x00, 0x3f, 0xff, 0x83, 0x87, 0x00, 0x00, 0x00, 0x7f, 0xff, 0x81, 0xc3, 0x80, 0x00, 
	0x00, 0xff, 0xff, 0x80, 0xe1, 0xc0, 0x00, 0x01, 0xff, 0xff, 0x80, 0x70, 0xe0, 0x00, 0x03, 0xff, 
	0xff, 0x88, 0x38, 0x70, 0x00, 0x07, 0xff, 0xff, 0x8c, 0x1c, 0x38, 0x00, 0x0f, 0xff, 0xff, 0x8e, 
	0x0e, 0x1c, 0x00, 0x1f, 0xff, 0xff, 0x8f, 0x07, 0x0e, 0x00, 0x3f, 0xff, 0xff, 0x8f, 0x83, 0x87, 
	0x00, 0x7f, 0xff, 0xff, 0x8f, 0xc1, 0xc3, 0x80, 0xff, 0xff, 0xff, 0x8f, 0xe0, 0xe1, 0xc0, 0xff, 
	0xff, 0xff, 0x8f, 0xe0, 0xe1, 0xc0, 0x7f, 0xff, 0xff, 0x8f, 0xc1, 0xc3, 0x80, 0x3f, 0xff, 0xff, 
	0x8f, 0x83, 0x87, 0x00, 0x1f, 0xff, 0xff, 0x8f, 0x07, 0x0e, 0x00, 0x0f, 0xff, 0xff, 0x8e, 0x0e, 
	0x1c, 0x00, 0x07, 0xff, 0xff, 0x8c, 0x1c, 0x38, 0x00, 0x03, 0xff, 0xff, 0x88, 0x38, 0x70, 0x00, 
	0x01, 0xff, 0xff, 0x80, 0x70, 0xe0, 0x00, 0x00, 0xff, 0xff, 0x80, 0xe1, 0xc0, 0x00, 0x00, 0x7f, 
	0xff, 0x81, 0xc3, 0x80, 0x00, 0x00, 0x3f, 0xff, 0x83, 0x87, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x87, 
	0x0e, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x8e, 0x1c, 0x00, 0x00, 0x00, 0x07, 0xff, 0x8c, 0x38, 0x00, 
	0x00, 0x00, 0x03, 0xff, 0x88, 0x70, 0x00, 0x00, 0x00, 0x01, 0xff, 0x80, 0xe0, 0x00, 0x00, 0x00, 
	0x00, 0xff, 0x81, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x83, 0x80, 0x00, 0x00, 0x00, 0x00, 0x3f, 
	0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x9c, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x07, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00
};

#define spriteWidth 50
#define spriteHeight 50

int spritePosX;
int spritePosY;

int displayWidth = 122;
int displayHeight = 250;



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
struct WordLengthPair{
  String word;
  int length;
};


//----------------------------------------------------------------------------------------------------



//Epaper Setup and Update Functions

void ePaperSetup(){
  display.init(115200, true, 2, false); // Waveshare reset circuit timing

  display.setRotation(2);
  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, 30);
    display.print("Ready to display!");
  } while (display.nextPage());

spritePosX = (display.width() - spriteWidth) / 2;
spritePosY = (display.height() - spriteHeight) / 2;

  display.hibernate();
}


vector<String> words;
vector<int> wordLengths;
int pos;
vector<WordLengthPair> textFormatting(String text){
   
    // Create a stringstream object
    stringstream ss(text.c_str());
    
    // Variable to hold each word
    string word;
    
    // Vector to store the words
    vector<string> words;
    
    // Extract words from the sentence
    while (ss >> word) {

      for (int i = 0; i < word.length(); i++) {
        Serial.println(word[i]);
      }
        // Add the word to the vector
        words.push_back(word);
    }

    uint16_t w, h;
    int16_t x1, y1;

    for (int i = 0; i < words.size(); i++){
      display.getTextBounds(words[i].c_str(), 0, 0, &x1, &y1, &w, &h);  // now filled in
      pos = (display.width() - w) / 2;
      wordLengths.push_back(pos);
      Serial.println(w);
      Serial.println(pos);
    }

    vector <WordLengthPair> wordLengthPairs;
    for (int i = 0; i < words.size(); i++) {
      WordLengthPair pair;
      pair.word = words[i].c_str();
      pair.length = wordLengths[i];
      wordLengthPairs.push_back(pair);
    }

    return wordLengthPairs;

    
}

void updateDisplay(String text, String color) {
  
  const char* printValue = text.c_str();
  display.init(115200, false); // false = don't re-run full reset, just wake it
  display.setFullWindow();
  display.firstPage();
  vector<WordLengthPair> wordLengthPairs = textFormatting(text);

  do {
    if (color.equals("black")){
      display.fillScreen(GxEPD_BLACK);

    }if (color.equals("red")){
      display.fillScreen(GxEPD_RED);
    }
    for (int i = 0; i < wordLengthPairs.size(); i++){
      display.setTextColor(GxEPD_WHITE);
      display.setCursor(wordLengthPairs[i].length, 30 + (i * 20));
      display.print(wordLengthPairs[i].word);
      
    }
    display.drawBitmap(spritePosX, spritePosY, epd_bitmap_telaeris_sprite, spriteWidth, spriteHeight, GxEPD_WHITE);
  } while (display.nextPage());

  display.hibernate(); // put it back to sleep until next update
}

//----------------------------------------------------------------------------------------------------
//WiFi Manager Callback Functions


void configModeCallback (WiFiManager *myWiFiManager) {
  #if DEBUG
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("WiFi mode: %d\n", WiFi.getMode());
    Serial.printf("Station count: %d\n", WiFi.softAPgetStationNum());
  #endif
}





void IPhandling(){
  if (WiFi.localIP().toString() != currentIP) {
    //updateDisplay("IP Address: ".toString());
    updateDisplay(WiFi.localIP().toString(), "black");
    currentIP = WiFi.localIP().toString();
  }
}


//----------------------------------------------------------------------------------------------------
//SQLite3 Callback Functions



int myCallback(void *data, int argc, char **argv, char **colNames) {
  for (int i = 0; i < argc; i++) {
    #if DEBUG
      Serial.printf("%s = %s\n", colNames[i], argv[i] ? argv[i] : "NULL");
    #endif
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
      #if DEBUG
        Serial.print(dbArr[i][j]);
      #endif
      if (j < dbArr[i].size() - 1) {
        #if DEBUG
          Serial.print(", ");
        #endif
      }
    }
    #if DEBUG
      Serial.println();
    #endif
  }
}

String isAllowedArr[2] = {"Not Found", "Not Found"};
int isAllowedCallback(void *data, int argc, char **argv, char **colNames) {
  
  #if DEBUG
    Serial.print(argv[1]);
    Serial.print(argv[2]);
  #endif
  isAllowedArr[0] = argv[1] ? argv[1] : "NULL"; // name
  isAllowedArr[1] = argv[2] ? argv[2] : "NULL"; // cardID

  #if DEBUG
    Serial.println(isAllowedArr[0]);
    Serial.println(isAllowedArr[1]);
  #endif


  return 0; // always continue normally
}

//----------------------------------------------------------------------------------------------------


void printFrame() {
  Serial.print("TagType=");
  Serial.print(tagType);
  Serial.print("  IDBitCount=");
  Serial.print(idBitCount);
  Serial.print("  ID=");
  for (int i = 0; i < idByteCount; i++) {
    //if (idBytes[i] < 0x10) Serial.print("0");
    Serial.print(idBytes[i], HEX);
  }
  Serial.println();
}

//----------------------------------------------------------------------------------------------------

void isAllowed(String cardID) {
  rc = sqlite3_exec(db, ("SELECT * FROM persons WHERE cardID = '" + cardID + "';").c_str(), isAllowedCallback, isAllowedArr, &errMsg);


  if (isAllowedArr[0].equals("Not Found")){
    updateDisplay("Card Not Found", "red");
    return;
    //TODO: Edit Update Display function to handle String
  }
  String test = "dominic";
  updateDisplay(isAllowedArr[0], "black");
  isAllowedArr[0] = "Not Found";
  isAllowedArr[1] = "Not Found";



  rc = 0;


  
}

vector<uint8_t> buffer;
String IDString = "";
void handleBytes(){
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    Serial.print(b, HEX);
    buffer.push_back(b);
  
    // process every complete line currently in the buffer
    while (true) {
    
      auto it = std::find(buffer.begin(), buffer.end(), '\n');
      if (it == buffer.end()) break;  // no complete line yet

      size_t lineLen = std::distance(buffer.begin(), it) + 1; // include the \n
      String line;
      line.reserve(lineLen);
      for (size_t i = 0; i < lineLen; i++) line += (char)buffer[i];
      line.trim();  // strips \r\n

      if (line.length() > 0) {
       
        // handle/parse `line` here
        Serial.println(line);  // echo back to the serial port
        display.println(line);
        isAllowed(line);
      }

      buffer.erase(buffer.begin(), buffer.begin() + lineLen); // consume just this line
    }

    // safety: if buffer grows too large with no terminator, drop it (noise/garbage)
    if (buffer.size() > MAX_BUFFER) {
      buffer.clear();
    }
  }
}


//-----------------------------------------------------------------------------------




WiFiManager wifiManager;

void setup() {


  Serial.begin(115200);  // USB, for debug output - open Serial Monitor at 115200
  Serial2.begin(TWN4_BAUD, SERIAL_8N1, TWN4_RX_PIN, TWN4_TX_PIN);
  


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
  #if DEBUG
    if (rc != SQLITE_OK) {
      Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db));
    } else {
      Serial.println("Database opened successfully");
    }
  #endif


  sqlite3_exec(db, "DROP TABLE IF EXISTS persons;", NULL, NULL, &errMsg);

  //sqlite3_exec(db, "DELETE FROM my_table;", NULL, NULL, &errMsg);
  rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS persons (id INTEGER, name TEXT, cardID TEXT);", NULL, NULL, &errMsg);
  #if DEBUG
    if (rc != SQLITE_OK) {
      Serial.printf("CREATE TABLE error: %s\n", errMsg);
      sqlite3_free(errMsg);
    }
  #endif

  sqlite3_exec(db, "DELETE FROM persons;", NULL, NULL, &errMsg);
  

  
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (1, 'Dominic', '0x38042969E2EE7A80');", NULL, NULL, &errMsg);
  #if DEBUG
    if (rc != SQLITE_OK) {
      Serial.printf("CREATE TABLE error: %s\n", errMsg);
      sqlite3_free(errMsg);
    }
  #endif
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (2, 'Franco', '0x40D29D0C19FEFF12E0');", NULL, NULL, &errMsg);
  #if DEBUG
    if (rc != SQLITE_OK) {
      Serial.printf("CREATE TABLE error: %s\n", errMsg);
      sqlite3_free(errMsg);
    }
  #endif
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (3, 'Andrew', '0x38048A5352FB7680');", NULL, NULL, &errMsg);
  #if DEBUG
    if (rc != SQLITE_OK) {
      Serial.printf("CREATE TABLE error: %s\n", errMsg);
      sqlite3_free(errMsg);
    }
  #endif
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (4, 'Raghav', '777');", NULL, NULL, &errMsg);
  #if DEBUG
    if (rc != SQLITE_OK) {
      Serial.printf("CREATE TABLE error: %s\n", errMsg);
      sqlite3_free(errMsg);
    }
  #endif
  rc = sqlite3_exec(db, "INSERT INTO persons (id, name, cardID) VALUES (5, 'Dominic', '888');", NULL, NULL, &errMsg);
  #if DEBUG
    if (rc != SQLITE_OK) {
      Serial.printf("CREATE TABLE error: %s\n", errMsg);
      sqlite3_free(errMsg);
    }
  #endif

 

  dbToArr();
  printDb();

  display.setTextColor(GxEPD_WHITE);
  updateDisplay("Ready to Display!", "black");

  Serial.println("Ready to Display!");
}

char str[MAX_STR_LEN];


void loop() {

  //IPhandling();


  
  handleBytes();

}