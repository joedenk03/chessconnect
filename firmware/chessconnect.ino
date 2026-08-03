//Joseph Denk, Brendan Jugan
//ChessConnect - Source Code
//April 28, 2025


//include necessary libraries

//Credentials (WiFi + Lichess API) - loaded from secrets.h (not tracked in git)
#include "secrets.h"
//wifi libraries
#include "WiFiS3.h"
#include "WiFiSSLClient.h"
#include "IPAddress.h"
//json parsing library
#include <ArduinoJson.h>
#include <Wire.h>
//lcd 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);
//stepper motor
#include <AccelStepper.h>
int once = 0;
//piece structure, used to keep board state internally, keeps piece type and color
struct Piece {
  char type;
  int color;
};
//game state
String state;
//illegal move var
int wrong = 0;
//electromagnet state
int emState = 0;
//to read responses
String resp;
//tracking board states w/ piece structs
Piece board[8][8];
Piece lastBoard[8][8];

String lastBoardMove="";
//to store reed switch states 
int squares[8][8];
int lastsquares[8][8];

//for promotions
int promotion = 0;
char promotionPiece = ' ';
// Define pin connections for steppers
const int dirPin1 = 13;
const int stepPin1 = 12;
const int dirPin2 = 11;
const int stepPin2 = 10;
//game info
String gameID = "";
String userColor = "";
//user color var
int userColorInt;
//calibration
int calibrate = 0;
// Define motor interface type
#define motorInterfaceType 1
int diagonalNum = 1;

// Creates an instance of steppers
AccelStepper stepperLeft(motorInterfaceType, stepPin1, dirPin1);
AccelStepper stepperRight(motorInterfaceType, stepPin2, dirPin2);
//defines steps to go for a square for - file movement, rank movement, diagonal movement
int stepsrank = 260;
int stepsfile = 260;
int stepsdiag = 530;
//limit switch states
int limitState1 = 0;
int limitState2 = 0;
//if user's move
int userMove;
//MUX pins
#define MUX0 4
#define MUX1 5
#define MUX2 6
#define MUX3 7
#define select0 A0
#define select1 A1
#define select2 A2
#define select3 A3
//used in move detection to find when a piece is picked up, to and from squares
int fromX = -1, fromY = -1, toX = -1, toY = -1;
String from = "";
String to = "";
String moveMade = "";
String lastSquare = "";
String id;
//opponent color
int otherColor = 0;
//wifi info
char ssid[]  = SECRET_WIFI_SSID;
char pass[]  = SECRET_WIFI_PASS;
//Lichess API key
char token[] = SECRET_LICHESS_TOKEN;
int status = WL_IDLE_STATUS;
char server[] = "lichess.org";  // name address for Lichess
int i = 0;
//gameStatus
String gameStatus = "";
String move;
String lastMove = "";
//initialize wifi client
WiFiSSLClient client;
//game started, UI variables for game selection, friend selection, time selection
int gameStarted = 0;
int gamemodeIndex = 0;
int timeIndex = 0;
String gamemodes[] = { "Friends", "Bot" };
String times[] = { "Unlimited", "180+30", "60+20", "30+20", "Back" };
String friends[] = { "brzzzzzzzzzzzzzzzz", "ChessConnect2025", "JOEDENK", "Back" };
int friendIndex = 0;
int selectingGamemode = 0;
int selectingFriends = 0;
int selectingTime = 0;
int onetime1 = 0, onetime2 = 0, onetime3 = 0;
//END VARIABLE DECLARATION

void initializeChessBoard() { //initialize board with pieces and colors
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      board[row][col].type = ' ';
      board[row][col].color = 0;
    }
  }

  // Set black major pieces (row 0)
  char majorPieces[8] = { 'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R' };
  for (int col = 0; col < 8; col++) {
    board[0][col].type = majorPieces[col];
    board[0][col].color = 3;
  }

  // Set black pawns (row 1)
  for (int col = 0; col < 8; col++) {
    board[1][col].type = 'P';
    board[1][col].color = 3;
  }

  // Set white pawns (row 6)
  for (int col = 0; col < 8; col++) {
    board[6][col].type = 'P';
    board[6][col].color = 2;
  }

  // Set white major pieces (row 7)
  for (int col = 0; col < 8; col++) {
    board[7][col].type = majorPieces[col];
    board[7][col].color = 2;
  }
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      lastBoard[i][j] = board[i][j];
    }
  }
} //end initializeChessBoard

void copyBoard(int revert) { //copy board state to lastboard
//revert var used to revert if there is an illegal move
//that will take it back to the lastboard state

//legal move, go to next board state
  if (revert == 0) {
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        lastBoard[i][j] = board[i][j];
      }
    }
  } 
  //revert board state
  else {
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        board[i][j] = lastBoard[i][j];
      }
    }
  }
} //end copyBoard



void moveFile(double x, int on) { 
  //move a certain number of squares -> x
  //file movement right: positive x, left: negative x
  //int on determines whether electromagnet is to be on or off during movement

  //if em currently not on, and to be turned on, turn on em
  if (on && emState == 0) {
    emOn();
  }
  //if em currently on, and to be turned off, turn off em
  if (on == 0 && emState == 1) emOff();

//steps to move based on one square movement of a file
  double stepsToMove = x * stepsfile;

  //move steppers to position
  stepperLeft.move(-stepsToMove);
  stepperRight.move(-stepsToMove);

  while (stepperLeft.distanceToGo() != 0 || stepperRight.distanceToGo() != 0) {
    stepperLeft.run();
    stepperRight.run();
  }

}//end moveFile



void moveRank(double x, int on) {
  //move a certain number of squares -> x
  //rank movement up: positive x, down: negative x
  //int on determines whether electromagnet is to be on or off during movement

  //if em currently not on, and to be turned on, turn on em
  if (on && emState == 0) {
    emOn();
  }
  //if em currently on, and to be turned off, turn off em
  if (on == 0 && emState == 1) emOff();
  //steps to move based on one square movement of a rank
  double stepsToMove = stepsrank * x;
  //move steppers to position
  stepperLeft.move(stepsToMove);
  stepperRight.move(-stepsToMove);

  while (stepperLeft.distanceToGo() != 0 || stepperRight.distanceToGo() != 0) {
    stepperLeft.run();
    stepperRight.run();
  }


  
}//end moveRank

void moveDiag(double x, int on) {
  //move a certain number of squares -> x
  //diagonal movement
  //int on determines whether electromagnet is to be on or off during movement
  //up right = negative
  //down left = positive

  //electromagnet control
  if (on && emState == 0) {
    emOn();
  }
  if (on == 0 && emState == 1) emOff();

  double stepsToMove = stepsdiag * x;
  stepperRight.move(stepsToMove);

  while (stepperRight.distanceToGo() != 0) {
    stepperRight.run();
    stepperLeft.stop();
  }

  diagonalNum++;
  //diagonalnum = number of diagonal moves made
  //every few moves, will recalibrate
}//end moveDiag

void moveDiag2(double x, int on) {
  //down right = negative
  //up left= positive

  if (on && emState == 0) {
    emOn();
  }
  if (on == 0 && emState == 1) emOff();
  double stepsToMove = stepsdiag * x;
  stepperLeft.move(stepsToMove);

  while (stepperLeft.distanceToGo() != 0) {
    stepperLeft.run();
    stepperRight.stop();
  }
  diagonalNum++;
}//end moveDiag2



void moveDiag2forCalibration(double x, int on) {
  //diagonal movement used for calibration purposes
  //down right = negative
  //up lef= positive

  if (on) {
    digitalWrite(9, HIGH);
    delay(1000);
  }
  double stepsToMove = 520 * -x;
  stepperLeft.move(stepsToMove);

  while (stepperLeft.distanceToGo() != 0) {
    stepperLeft.run();
  }


  emOff();
}//end moveDiag2forCalibration



void runCalibration() {

  //calibration function for electromagnet

  //read limit switches
  int limitState1 = digitalRead(2);
  int limitState2 = digitalRead(3);
  //while first limit switch not hit
  while (limitState1 == HIGH) {
    //keep reading and moving towards it until hit
    limitState1 = digitalRead(2);
    stepperLeft.setSpeed(-1000);

    stepperRight.setSpeed(1000);
    stepperLeft.runSpeed();
    stepperRight.runSpeed();
  }
  //stop 
  stepperLeft.setSpeed(0);
  stepperRight.setSpeed(0);
  stepperLeft.runSpeed();
  stepperRight.runSpeed();
  delay(100);
//while second limit switch not hit
  while (limitState2 == HIGH) {
    //keep reading and moving towards it until hit
    limitState2 = digitalRead(3);
    stepperLeft.setSpeed(-1000);
    stepperRight.setSpeed(-1000);
    stepperLeft.runSpeed();
    stepperRight.runSpeed();
  }
  //stop
  stepperLeft.setSpeed(0);
  stepperRight.setSpeed(0);
  stepperLeft.runSpeed();
  stepperRight.runSpeed();
  //move slightly in towards the middle of the square more - for more accurate movement to D5
  stepperLeft.move(-18);
  stepperRight.move(-18);

  while (stepperLeft.distanceToGo() != 0 || stepperRight.distanceToGo() != 0) {
    stepperLeft.run();
    stepperRight.run();
  }
  delay(100);
}//end runCalibration

void goToD5() {
//gets electromagnet to d5 home square

//calibrate
  runCalibration();
//go to top left square
  moveRank(1.8, 0);
//move diagonally to d5
  moveDiag2forCalibration(-3, 0);
}//end goToD%

void emOn() {
  //turn electromagnet on
  digitalWrite(9, HIGH);
  emState = 1;
  delay(500);
} //end emOn

void emOff() {
  //turn electromagnet off
  digitalWrite(9, LOW);
  emState = 0;
} //end emOff

void displayGamemode() {
  //used for user interface, display gamemode
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select Gamemode:");
  lcd.setCursor(0, 1);
  lcd.print(gamemodes[gamemodeIndex]);
  lcd.setCursor(0, 2);
  lcd.print("Black to Scroll");
  lcd.setCursor(0, 3);
  lcd.print("White to Continue");
} //end displayGamemode

void displayTime() {
  //used for user interface, display time control
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select Time Control:");
  lcd.setCursor(0, 1);
  if (gamemodeIndex == 2 && timeIndex == 0) timeIndex++;
  lcd.print(times[timeIndex]);
  lcd.setCursor(0, 2);
  lcd.print("Black to Scroll");
  lcd.setCursor(0, 3);
  lcd.print("White to Continue");
} //end displayTime

void displayFriends() {
  //used for user interface, display friend
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select Friend:");
  lcd.setCursor(0, 1);
  lcd.print(friends[friendIndex]);
  lcd.setCursor(0, 2);
  lcd.print("Black to Scroll");
  lcd.setCursor(0, 3);
  lcd.print("White to Continue");
} //end displayFriends

void displayGameInfo() {
  //used for user interface, display game info after a game has been seleted
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(gamemodes[gamemodeIndex]);
  lcd.setCursor(0, 1);
  lcd.print(times[timeIndex]);
  if (gamemodeIndex == 0) {
    lcd.setCursor(0, 2);
    lcd.print(friends[friendIndex]);
  }
  lcd.setCursor(0, 3);
  lcd.print("Request Sent-Waiting");
} //end displayGameInfo

void makeAPIRequest(String req) {
  //used to make any GET api request to server
  if (client.connect(server, 443)) {
    Serial.print("Requesting: ");
    Serial.println(req);

    client.print("GET ");
    client.print(req);
    client.println(" HTTP/1.1");
    client.println("Host: lichess.org");
    client.println("Connection: close");
    client.print("Authorization: Bearer ");
    client.println(token);
    client.println();

  }

  else Serial.println("Not connected to server");
}//end makeAPIRequest

void postAPIRequest(String req, String Post) {

  //used to post any api request to server
  //String Post used to add any additional info to api request - used when starting a game with certain conditions
  if (client.connect(server, 443)) {
    Serial.print("Posting: ");
    Serial.println(req);

    client.print("POST ");
    client.print(req);
    client.println(" HTTP/1.1");
    client.println("Host: lichess.org");
    client.println("Connection: close");
    client.print("Authorization: Bearer ");
    client.println(token);
    if (Post.length() != 0) {
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(Post.length());
    }

    client.println();
    if (Post.length() != 0) {
      client.println(Post);
    }



    Serial.println("Request sent.");
  } else Serial.println("Not connected to server");
}//end postAPIRequest


String toSquare(int row, int col) { 
  //convert 2D array index to a chess square on the board
  //used to take movements on array to moves to send to the game
  char file = 'a' + col;
  char rank = '8' - row;

  return String(file) + String(rank);
}//end toSquare


void initializeArray() {
  //initialize squares and lastsquares arrays
  for (int a = 0; a < 8; a++) {
    for (int b = 0; b < 8; b++) {
      lastsquares[a][b] = 1;
      squares[a][b] = 1;
    }
  }
}//end initializeArray

void fillPrevArray() {
  //fill previous array for reading reed switches
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      lastsquares[i][j] = squares[i][j];
    }
  }
}//end fillPrevArray


void printBoard() {
  //used for debugging and visualization
  //prints board state during games 
  //shows each piece type and color
  Serial.println("  a  b  c  d  e  f  g  h");
  for (int row = 0; row < 8; row++) {
    Serial.print(8 - row);  // Print rank number
    Serial.print(" ");
    for (int col = 0; col < 8; col++) {
      Piece p = board[row][col];
      if (p.color == 2) {
        Serial.print("W");
        Serial.print(p.type);
      } else if (p.color == 3) {
        Serial.print("B");
        Serial.print(p.type);
      } else {
        Serial.print(" . ");
        continue;
      }
      Serial.print(" ");
    }
    Serial.print(" ");
    Serial.println(8 - row);  // Mirror rank on right
  }
  Serial.println("  a  b  c  d  e  f  g  h");
} //end printBoard

void printLastBoard() {
//also used for debugging, prints last board state
  Serial.println("  a  b  c  d  e  f  g  h");
  for (int row = 0; row < 8; row++) {
    Serial.print(8 - row);  // Print rank number
    Serial.print(" ");
    for (int col = 0; col < 8; col++) {
      Piece p = lastBoard[row][col];
      if (p.color == 2) {
        Serial.print("W");
        Serial.print(p.type);
      } else if (p.color == 3) {
        Serial.print("B");
        Serial.print(p.type);
      } else {
        Serial.print(" . ");
        continue;
      }
      Serial.print(" ");
    }
    Serial.print(" ");
    Serial.println(8 - row);  // Mirror rank on right
  }
  Serial.println("  a  b  c  d  e  f  g  h");
}//end printLastBoard

void fillArray() {
  //reading reed switches to fill array to check for differences
  int a;
  int arrayrow;
  for (int b3 = 0; b3 <= 1; b3++) {  // Most significant bit
    for (int b2 = 0; b2 <= 1; b2++) {
      for (int b1 = 0; b1 <= 1; b1++) {
        for (int b0 = 0; b0 <= 1; b0++) {  // Least significant bit
          digitalWrite(A0, b0);
          digitalWrite(A1, b1);
          digitalWrite(A2, b2);
          digitalWrite(A3, b3);
          delay(10);
          int arraycol = 7 - ((b0) | (b1 << 1) | (b2 << 2));  //get the array column from the specific muxes used
          arrayrow = 7 - (((0 + 1) * 2) - b3 - 1);            //get array rows, 4 different because reading four switches each

          squares[arrayrow][arraycol] = digitalRead(4); //fill array

          arrayrow = 7 - (((1 + 1) * 2) - b3 - 1);  

          squares[arrayrow][arraycol] = digitalRead(5);

          arrayrow = 7 - (((2 + 1) * 2) - b3 - 1);  

          squares[arrayrow][arraycol] = digitalRead(6);

          arrayrow = 7 - (((3 + 1) * 2) - b3 - 1);  

          squares[arrayrow][arraycol] = digitalRead(7);
        }
      }
    }
  }
}//end fillArray



/* -------------------------------------------------------------------------- */
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network.
    status = WiFi.begin(ssid, pass);

    // wait for connection:
    delay(1000);
  }

  printWifiStatus();

  Serial.println("\nStarting connection to server...");
//initialize pins

  //buzzer
  pinMode(8, OUTPUT);
  //mux outputs
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  //mux selects
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  //electromagnet
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
  //limit switches & buttons
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
//initialize LCD, startup sequence
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ChessConnect");
  lcd.setCursor(1, 1);
  lcd.print("Brendan Jugan");
  lcd.setCursor(1, 2);
  lcd.print("Joseph Denk");
  lcd.setCursor(0, 3);
  lcd.print("Senior Project");
  delay(2000);

  initializeArray();
  //set stepper speeds/accelerations
  stepperLeft.stop();
  stepperRight.stop();
  stepperLeft.setMaxSpeed(1000);
  stepperRight.setMaxSpeed(1000);
  stepperLeft.setAcceleration(900);
  stepperRight.setAcceleration(900);


//lichess root certificate
  const char* root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
    "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
    "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
    "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
    "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
    "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
    "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
    "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
    "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
    "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
    "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
    "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
    "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
    "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
    "-----END CERTIFICATE-----\n";
  client.setCACert(root_ca);
  //start UI with select gamemode
  selectingGamemode = 1;
  //recalibrate
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Recalibrating...");
  lcd.setCursor(0, 1);
  lcd.print("Please do not");
  lcd.setCursor(0, 2);
  lcd.print("press buttons!");

//go to home square
  goToD5();
  lastSquare = "d5";
  digitalWrite(9, LOW);
  delay(200);
}


void boardMove() {
  //used in USERS MOVE
  //looks for a move made on the board


  // Serial.println("Entered BOARDMOVE function");
  int taking = 0;
//determine colors
  if (userColorInt == 2) otherColor = 3;
  else otherColor = 2;
  //prompt user move
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(userColor + " to move!");
  lcd.setCursor(0, 1);
  lcd.print("Opponent Button");
  lcd.setCursor(0, 2);
  lcd.print("for options");
  delay(1000);
  //if not first move, aka last move made not ""
  if (moveMade != "") {
    fillPrevArray();
    fillArray();
  }

  while (1) {
    //keep scanning
    //Serial.println("IN WHILE!");
    fillPrevArray();
    fillArray();

    delay(100);

    if (digitalRead(otherColor) == LOW) { 
       //options menu
      int select = 0;
      int back = 0;
      while (back == 0) {
        if (select == 0) { //resignation option
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("To Resign:");
          lcd.setCursor(0, 1);
          lcd.print("your button");
          lcd.setCursor(0, 2);
          lcd.print("To scroll");
          lcd.setCursor(0, 3);
          lcd.print("opponent button");
          delay(300);
          while (digitalRead(otherColor) == HIGH && digitalRead(userColorInt) == HIGH) {
          }//wait for input
          if (digitalRead(userColorInt) == LOW) { //resign game
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Resigning Game");
            postAPIRequest("/api/board/game/" + gameID + "/resign", ""); //send resignation
            delay(1000);
            String theResp = read_JSON();
            Serial.println(theResp); //debug
            gameStarted = 0; //game ended, will send user back to selection screen
            break;
          } else if (digitalRead(otherColor) == LOW) { //user scrolled, adjust pieces option
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("To Adjust Pieces:");
            lcd.setCursor(0, 1);
            lcd.print("your button");
            lcd.setCursor(0, 2);
            lcd.print("To scroll");
            lcd.setCursor(0, 3);
            lcd.print("opponent button");
            delay(300);
            select++;
          }
        }  //end select 0
        else if (select == 1) {
          while (digitalRead(otherColor) == HIGH && digitalRead(userColorInt) == HIGH) {
          }
          if (digitalRead(userColorInt) == LOW) { //adjust pieces
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Adjust Pieces");
            lcd.setCursor(0, 1);
            lcd.print("Press to Continue");
            back = 1;
            delay(1000);
            while (digitalRead(userColorInt) == HIGH && digitalRead(otherColor) == HIGH) {
              fillArray();
              fillPrevArray();
            }
            delay(300);
          } else if (digitalRead(otherColor) == LOW) { //user scrolled, back option
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("To go back:");
            lcd.setCursor(0, 1);
            lcd.print("your button");
            lcd.setCursor(0, 2);
            lcd.print("To scroll");
            lcd.setCursor(0, 3);
            lcd.print("opponent button");
            delay(300);
            select++;
          }
        } else if (select == 2) {
          while (digitalRead(otherColor) == HIGH && digitalRead(userColorInt) == HIGH) {
          }
          if (digitalRead(userColorInt) == LOW) {
            back = 1;
            delay(300);
          } else if (digitalRead(otherColor) == LOW) {
            select = 0;
            delay(300);
          }
        }
      }  //end while back == 0
      if (gameStarted == 0) break; //if game over (resigned) break
      else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(userColor + " to move!");
        lcd.setCursor(0, 1);
        lcd.print("Opponent Button");
        lcd.setCursor(0, 2);
        lcd.print("for options");
      }
    }  //end options if


    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        if (squares[i][j] != lastsquares[i][j]) {
          if (lastsquares[i][j] != 1 && squares[i][j] == 1 && fromX == -1 && fromY == -1) {
            // Found departure square
            fromX = i;
            fromY = j;
            lcd.clear();
            from = toSquare(i, j);
            lcd.setCursor(0, 0);
            lcd.print(userColor);
            lcd.print(" to move");
            lcd.setCursor(0, 1);
            lcd.print(from + " picked up");
            if (board[i][j].color == otherColor) { //if piece picked up firstly is not users - means user is taking
              taking = 1;
              lcd.setCursor(0, 3);
              lcd.print("TAKING");
            }
          }
          if (lastsquares[i][j] == 1 && squares[i][j] != 1 && fromX == i && fromY == j) {
            if (taking == 1) taking = 0;
            //piece put back
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(userColor + " to move!");
            lcd.setCursor(0, 1);
            lcd.print("Opponent Button");
            lcd.setCursor(0, 2);
            lcd.print("for options");
            fromX = -1;
            fromY = -1;
          }
          if (lastsquares[i][j] == 1 && squares[i][j] != 1 && fromX != -1 && fromY != -1 && taking == 0) {  //if a new piece is put down AND there is a piece that has departed
            // Found destination square
            toX = i;
            toY = j;
            to = toSquare(i, j);

            lcd.setCursor(6, 1);
            lcd.print(to + " ");
          }
          if (lastsquares[i][j] != 1 && squares[i][j] == 1 && (fromX != i || fromY != j) && taking == 1 && board[i][j].color == userColorInt) {  //another piece picked up to take
            toX = fromX;
            toY = fromY;
            fromX = i;
            fromY = j;
            to = toSquare(toX, toY);
            from = toSquare(i, j);
            Serial.println("FROMX STUFF:" + String(fromX) + String(fromY) + String(toX) + String(toY)); //debug
            lcd.setCursor(8, 3);
            lcd.print(from + to);
            //meaning piece is being taken

            while (squares[toX][toY] == 1 && digitalRead(otherColor) == HIGH) {
              fillPrevArray();
              fillArray();
            } //wait until piece put down

            if (digitalRead(otherColor) == LOW) { //user - undo move
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Put pieces back");
              taking = 0;
              while (squares[fromX][fromY] == 1 && squares[toX][toY] == 1) {//wait to be put back
                fillArray();
              }
              fillPrevArray();
              fromX = fromY = toX = toY = -1; //reset vars

            }  //end button pressed if

          }  //end another piece
        }    //end square mismatch
      }
    }

    if (fromX != -1 && fromY != -1 && toX != -1 && toY != -1) {
      //MOVE MADE
      moveMade = from + to;
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("MOVE MADE: ");
      lcd.setCursor(0, 2);
      lcd.print(moveMade);



      while (1) {
        if (digitalRead(userColorInt) == LOW) {  //user color pressed - sent move


          delay(100);

          //SPECIAL CASES
          //--------------------------------------------------------------------------------------------------------------
          if (abs(toY - fromY) > 1 && board[fromX][fromY].type == 'K') { // if king moving more than a square ever - means user castled
            castling(moveMade);
            Serial.println("Castling here");
          } else {
            if (board[toX][toY].type == ' ' && board[fromX][fromY].type == 'P' && toY - fromY != 0 && toX - fromX != 0) {
              //en passant
              if (toY - fromY > 1) {  //moving right
                board[fromX][fromY + 1].type = ' ';
                board[fromX][fromY + 1].color = 0;
              } else {  //moving left
                board[fromX][fromY - 1].type = ' ';
                board[fromX][fromY - 1].color = 0;
              }
            }
            //--------------------------------------------------------------------------------------------------------------
            //end special cases
            //set new square of piece to user color and piece to indicate what moved there
            board[toX][toY].color = userColorInt;
            board[toX][toY].type = board[fromX][fromY].type;
            if (board[toX][toY].type == 'P' && userColorInt == 2 && moveMade.charAt(3) == '8') { //promotion
              board[toX][toY].type = 'Q';
              moveMade = moveMade + "q";
            } else if (board[toX][toY].type == 'P' && userColorInt == 3 && moveMade.charAt(3) == '1') { //promotion - black
              board[toX][toY].type = 'Q';
              moveMade = moveMade + "q";
            }

            //piece moved off square - now no piece there
            board[fromX][fromY].color = 0;
            board[fromX][fromY].type = ' ';
          }
          fromX = fromY = toX = toY = -1; //reset

          return;
        }

        if (digitalRead(otherColor) == LOW) {  //other color pressed - undo move
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Deleted: " + moveMade);
          lcd.setCursor(0, 1);
          lcd.print("Return piece(s)");
          lcd.setCursor(0, 2);
          lcd.print("to square(s)");

          //wait until pieces are put back
          fillArray();
          if (taking == 0) {
            while (squares[fromX][fromY] == 1) {
              fillArray();
            }
          } else {
            while (squares[fromX][fromY] == 1 && squares[toX][toY] == 1) {
              fillArray();
            }
            taking = 0;
          }
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(userColor);
          lcd.print(" to Move");
          fromX = fromY = toX = toY = -1;

          break;
        }  //end other pressed
      }    //end button while
    }      //end move made loop
  }        //end overall while 1
}  //end function boardMove

String read_JSON() { //read JSON from client
  String s = "";
  while (client.available()) {
    char c = client.read();
  
    s = s + c;
  }
  return s; //return string from client
}//end read_JSON

void read_response() {
  /* -------------------------------------------------------------------------- */
  uint32_t received_data_num = 0;
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    return;
  } //if nothing returned, request did not send
  while (client.available()) {
    /* actual data reception */
    char c = client.read();
    /* print data to serial port */
    //Serial.print(c);
    /* wrap data to 80 columns*/
    received_data_num++;
    if (received_data_num % 80 == 0) {
      Serial.println();
    }
  }
}//end read_response

String getColorAI(String c) { //function to get the color of the user using the server response , used only for bot games
//in JSON response, different ways to get color for bot vs normal opponent games

  StaticJsonDocument<768> doc;

  
  const char* input = c.c_str();
  DeserializationError error = deserializeJson(doc, input);
//deserialize json
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return "ERROR";
  }
  const char* color = doc["player"]; //color is listed as "player" in JSON for bot games
  return String(color);
}///end getColorAI

String getColor(String c) { //function to get the color of the user from server response
  StaticJsonDocument<768> doc;


  const char* input = c.c_str();
  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return "ERROR";
  }

  const char* color = doc["finalColor"]; //color of the user is in JSON as "finalColor"
  return String(color);
}//end getColor

String getStatus(String c) { //function to get the status of the game (checkmate, draw, etc)
  StaticJsonDocument<1024> doc;

  String cleanResponse = c.substring(1);
  while (cleanResponse.charAt(0) != '{') {
    cleanResponse = cleanResponse.substring(1);
  } //isolate the JSON response part

  const char* input = cleanResponse.c_str();
  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return "ERROR";
  }
  JsonObject state = doc["state"]; //get state JSON
  const char* state_status = state["status"];  //find the status from the state JSON
  if (String(state_status).length() < 3) state_status = doc["status"]; //if length < 3, then status is elsewhere in response

  return String(state_status);
} //end getStatus

String getWinner(String c) { //function to get the winner of the game based on server response 
  StaticJsonDocument<1024> doc;

  String cleanResponse = c.substring(1);
  while (cleanResponse.charAt(0) != '{') {//isolate the JSON response
    cleanResponse = cleanResponse.substring(1);
  }
  
  const char* input = cleanResponse.c_str();
  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return "ERROR";
  }
  JsonObject state = doc["state"];
  const char* state_status = state["winner"];
  if (String(state_status).length() < 3) state_status = doc["winner"]; //determine the winner based on the JSON

  return String(state_status);
} //end getWinner


String getID(String c) { //function to get the ID of the game based on server response 
  StaticJsonDocument<768> doc;

  const char* input = c.c_str();
  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return "ERROR";
  }


  const char* id = doc["id"]; //extract ID from JSON
  return String(id);
} //end getID

String getRecentMove(String c) {  //function to get the most recent move based on server response 


  StaticJsonDocument<1024> doc;

  String cleanResponse = c.substring(1); 
  while (cleanResponse.charAt(0) != '{') { //isolate JSON
    cleanResponse = cleanResponse.substring(1);
  }

  const char* input = cleanResponse.c_str();
  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return "ERROR";
  }
  //get the string of all moves made during the game firstly
  const char* moves = doc["moves"];  // Moves field is directly at the root level, not inside "state"
  int firstResp = 0;
  if (!moves || String(moves).length() == 0) {
    moves = doc["state"]["moves"];

    if (!moves || String(moves).length() == 0) {
      return "N/A";  // No moves yet or game just started
    }
    firstResp = 1;
  }

  // Convert moves string to a String object
  String moveStr = String(moves);

  // Find the last space in the moves string
  //this is used to isolate where the last move is in the string of all the moves
  int lastSpace = moveStr.lastIndexOf(' ');

  // Extract the last move
  if (lastSpace == -1) {
    // If there's no space, it's just a single move
    return moveStr;  // Only one move exists
  } else {
    // Return the last move after the last space
    return moveStr.substring(lastSpace + 1);  // Extract last move
  }
}//end getRecentMove


int initialBoardState() { //function when starting a game to ensure that user gets all pieces on the correct squares on the board
  String black = "White: ";
  String white = "Black: "; //used in printing 
  int maxLength = 20;
  int badb = 0;
  int badw = 0;
  for (int row = 0; row < 2; row++) {  // First two rows
    for (int col = 0; col < 8; col++) {
      if (squares[row][col] != 0) {
        if ((white.length() + 3) < 20) white = white + toSquare(row, col) + " "; //print the squares still needed to have a piece put on them for black
        badb = 1; //black stil not yet set with pieces
      }
    }
  }

  for (int row = 6; row < 8; row++) {  // Last two rows
    for (int col = 0; col < 8; col++) {
      if (squares[row][col] != 0) {
        if ((black.length() + 3) < 20) black = black + toSquare(row, col) + " "; //print the squares still needed to have a piece put on them for whites
        badw = 1; //white still not yet set with pieces
      }
    }
  }

  //notify user of what pieces are not yet detected
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Put all pieces");
  lcd.setCursor(0, 1);
  lcd.print("on their squares");
  lcd.setCursor(0, 2);
  if (badb == 1) lcd.print(white); //if still not all black pieces detected, print where
  else lcd.print("Black is good"); //else good
  lcd.setCursor(0, 3);
  if (badw == 1) lcd.print(black); //if still not all white pieces detected, print where
  else lcd.print("White is good"); //else good
  delay(300);
  return badb + badw;  // All first two and last two rows are 0
} //end initialBoardState


String getDecision(String c) { //function used to determine whether a challenge has been accepted or not 

  StaticJsonDocument<32> doc;
  const char* input = c.c_str();
  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return "ERROR";
  }

  const char* done = doc["done"];  // "accepted" if accepted
  return String(done);
} //end getDecision


void enPassant(String en) { //used for special move en passant
  int d5file = 'd' - 'a' + 1;
  int d5rank = '5' - 'a' + 1;
  int row = '8' - en.charAt(3);
  int rowFrom = '8' - en.charAt(1);
  int col = en.charAt(2) - 'a';
  int colFrom = en.charAt(0) - 'a';
  int file1 = en.charAt(0) - 'a' + 1;
  int file2 = en.charAt(2) - 'a' + 1;
  int rank1 = en.charAt(1) - 'a' + 1;
  int rank2 = en.charAt(3) - 'a' + 1;
  int rankMovement = file2 - file1;
  int fileMovement = rank2 - rank1;
  int pawnRank = rank1; //pawn rank is the original rank its coming from
  int pawnFile;
  int pawnRow = rowFrom;
  int pawnCol;
  if (rankMovement > 0) {
    //taking to the right of the pawn
    pawnFile = file1 + 1; //meaning the pawn being taking is just to the right of the pawn taking 
    pawnCol = colFrom + 1;

  } else {
    //to the left
    pawnFile = file1 - 1;
    pawnCol = colFrom - 1;
  }
  //this pawn, although not on the destination square, has been captured
  //so set its square in the board state to now no piece
  board[pawnRow][pawnCol].type = ' ';
  board[pawnRow][pawnCol].color = 0; 

  //update destination square and origin square
  board[row][col].color = board[rowFrom][colFrom].color;
  board[row][col].type = 'P';
  board[rowFrom][colFrom].type = ' ';
  board[rowFrom][colFrom].color = 0;

  if (userMove == 0) { //if user is en passantning
    moveFile(pawnRank - d5rank, 0);
    moveRank(pawnFile - d5file, 0); //move to the pawn being taken
    //move pawn to outside of board - captured
    moveRank(-0.6, 1);
    moveFile(d5rank - pawnRank - 0.4, 1);

    moveRank(-pawnFile, 1);

    //it should be outside of board to indicate capture
    //now move back to d5, essentially undo everything
    //the rest of the movement is handled as normal during movement outside of this function
    emOff();
    delay(200);
    moveRank(file1 + 0.6, 0);
    moveFile(0.4 + rank1 - d5rank, 0);
  }
} //end enPassant

void castling(String cas) { //function for special move - castle
  int d5file = 'd' - 'a' + 1;
  int d5rank = '5' - 'a' + 1;
  int row = '8' - cas.charAt(3);
  int rowFrom = '8' - cas.charAt(1);
  int col = cas.charAt(2) - 'a';
  int colFrom = cas.charAt(0) - 'a';
  int file1 = cas.charAt(0) - 'a' + 1;
  int file2 = cas.charAt(2) - 'a' + 1;
  int rank1 = cas.charAt(1) - 'a' + 1;
  int rank2 = cas.charAt(3) - 'a' + 1;
  if (cas == "e8g8") { //four castling cases, this is black kingside
    board[rowFrom][colFrom].color = 0;
    board[rowFrom][colFrom].type = ' ';
    //king moves to g8
    board[row][col].type = 'K';
    board[row][col].color = userMove == 0 ? otherColor : userColorInt; 
    ;
    //rook also moves to the opposite side of the king (f8)
    board[0][7].color = 0;
    board[0][7].type = ' ';
    board[0][5].type = 'R';
    board[0][5].color = userMove == 0 ? otherColor : userColorInt;
    ;

  } else if (cas == "e8c8") {  // Black queenside
    board[rowFrom][colFrom].color = 0;
    board[rowFrom][colFrom].type = ' ';
    board[row][col].type = 'K';
    board[row][col].color = userMove == 0 ? otherColor : userColorInt;
    board[0][0].color = 0;
    board[0][0].type = ' ';
    board[0][3].type = 'R';
    board[0][3].color = userMove == 0 ? otherColor : userColorInt;
  } else if (cas == "e1g1") {  // White kingside
    board[rowFrom][colFrom].color = 0;
    board[rowFrom][colFrom].type = ' ';
    board[row][col].type = 'K';
    board[row][col].color = userMove == 0 ? otherColor : userColorInt;
    board[7][7].color = 0;
    board[7][7].type = ' ';
    board[7][5].type = 'R';
    board[7][5].color = userMove == 0 ? otherColor : userColorInt;
  }

  else if (cas == "e1c1") {  // White queenside
    board[rowFrom][colFrom].color = 0;
    board[rowFrom][colFrom].type = ' ';
    board[row][col].type = 'K';
    board[row][col].color = userMove == 0 ? otherColor : userColorInt;
    board[7][0].color = 0;
    board[7][0].type = ' ';
    board[7][3].type = 'R';
    board[7][3].color = userMove == 0 ? otherColor : userColorInt;
  }


//if opponent castled, must move their rook and king to their new positions
  if (userMove == 0) {
    if (cas == "e8g8") {
      //firstly move the king over to the new square
      moveFile(rank1 - d5rank, 0);
      moveRank(file1 - d5file, 0);
      moveRank(2, 1);
      //now move to the rook square
      moveRank(1, 0);
      //take the rook up to the edge of the squares to move it past the king 
      moveFile(-0.6, 1);
      moveRank(-2, 1);
      //move back onto the middle of the square
      moveFile(0.6, 1);
      moveRank(1, 0);  //rook is on the new square
    }
    if (cas == "e1g1") {
      //firstly move the king over to the new square
      moveFile(rank1 - d5rank, 0);
      moveRank(file1 - d5file, 0);
      moveRank(2, 1);
      //now move to the rook square
      moveRank(1, 0);
       //take the rook up to the edge of the squares to move it past the king 
      moveFile(0.6, 1);
      moveRank(-2, 1);
      //move back onto the middle of the square
      moveFile(-0.6, 1);
      moveRank(1, 0);
    }
    //same methodology applied for all castling moves, just with different distances depending on kingside or queenside
    if (cas == "e1c1") {
      moveFile(rank1 - d5rank, 0);
      moveRank(file1 - d5file, 0);
      moveRank(-2, 1);
      moveRank(-2, 0);
      moveFile(0.6, 1);
      moveRank(3, 1);
      moveFile(-0.6, 1);
      moveRank(-1, 0);
    }
    if (cas == "e8c8") {
      moveFile(rank1 - d5rank, 0);
      moveRank(file1 - d5file, 0);
      moveRank(-2, 1);
      moveRank(-2, 0);
      moveFile(-0.6, 1);
      moveRank(3, 1);
      moveFile(0.6, 1);
      moveRank(-1, 0);
    }
  }
} //end castling



/* -------------------------------------------------------------------------- */
void printWifiStatus() {
  /* -------------------------------------------------------------------------- */
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
} //end printWifiStatus



/* -------------------------------------------------------------------------- */
void loop() {
  /* -------------------------------------------------------------------------- */



  while (gameStarted == 0) { //if game not started, game selection by the user 


    if (selectingGamemode == 1) { //gamemode selection first
      if (onetime1 == 0) { //used to initially display gamemodes before scrolling/selection
        displayGamemode();
        onetime1++;
      }
      if (digitalRead(3) == LOW) {  // Black button scrolls options
        delay(200);
        gamemodeIndex = (gamemodeIndex + 1) % 2; //change displayed gamemode
        displayGamemode();
      }
      if (digitalRead(2) == LOW) {  // White button selects option
        delay(200);
        if (gamemodeIndex == 0) { //user selected friend game

          selectingGamemode = 0;
          selectingFriends = 1; 
        }

        else { //user selected bot
          selectingGamemode = 0;
          selectingTime = 1;
        }
      }
    } //end selectingGamemode == 1


    if (selectingFriends == 1) { //user selecting a friend
      if (onetime2 == 0) { //used to initially display friends before scrolling/selection
        displayFriends();
        onetime2++;
      }
      if (digitalRead(3) == LOW) {  // Black button scrolls options
        delay(200);
        friendIndex = (friendIndex + 1) % 4; //display next friend
        displayFriends();
      }
      if (digitalRead(2) == LOW) {  // White button selects option
        delay(200);
        if (friendIndex == 3) { //Bcck button, revert to previous selection screen
          selectingGamemode = 1;
          selectingFriends = 0;
          gamemodeIndex = 0;
          friendIndex = 0;
          onetime1 = 0;
          onetime3 = 0;
          onetime2 = 0;
        } else { //friend selected, time control selection now
          selectingFriends = 0;
          selectingTime = 1;
        }
      }
    } //end selectingFriends == 1



    if (selectingTime == 1) { //user selecting the time control 
      if (onetime3 == 0) { //used to initially display time controls before scrolling/selection
        displayTime();
        onetime3++;
      }
      if (digitalRead(3) == LOW) {  // Black button scrolls options
        delay(200);
        timeIndex = (timeIndex + 1) % 5;

        displayTime();
      }
      if (digitalRead(2) == LOW) { //white button to select
        delay(200);
        if (timeIndex == 4) { //BACK pressed
          if (gamemodeIndex == 0) { //if friend game selected, need to go back to friend selection rather than game mode selection because of the extra screen

            selectingTime = 0;
            selectingFriends = 1;
            onetime1 = 0;
            onetime3 = 0;
            onetime2 = 0;
            timeIndex = 0;
            friendIndex = 0;
          } else { //go back to gamemode selection
            selectingTime = 0;
            selectingGamemode = 1;
            onetime1 = 0;  //onetime1
            onetime3 = 0;
            onetime2 = 0;
            gamemodeIndex = 0;
            timeIndex = 0;
          }
        } 
        else { //not back 

          int piecesNotInPosition = 1;

          while (piecesNotInPosition > 0) { //WHILE the pieces on the board are not in the starting position
          //if pieces are not detected on starting squares keep looping and updating the remaining squares
          //display remaining undetected squares using the initialBoardState function 
            fillArray(); //keep filling array 
            piecesNotInPosition = initialBoardState();
          }

          delay(300);
          lcd.clear();
          displayGameInfo(); //display the game info to the user
         
         
          //GAME REQUEST TO BE SENT

          String Post = ""; //any info to be sent to server, like time controls etc
          //clock.limit - total time each side gets to start
          //clock.increment - time increment
          //keepAliveStream - keeps request alive to give a response for when the opponent accepts or declines or times out of challenge
          if (timeIndex == 2) {
            Post = Post + "clock.limit=3600&clock.increment=20&keepAliveStream=true";
          } else if (timeIndex == 3) {
            Post = Post + "clock.limit=1800&clock.increment=20&keepAliveStream=true";
          } else if (timeIndex == 1) {
            Post = Post + "clock.limit=10800&clock.increment=30&keepAliveStream=true";
          } else Post = "keepAliveStream=true";
          if (gamemodeIndex == 0) {  //FRIENDLY GAME


            postAPIRequest("/api/challenge/" + friends[friendIndex], Post); //send the api request
            delay(500);
            
            String c = read_JSON();
            
            String jsonPart = "";
            int startIndex = c.indexOf('{');
            if (startIndex != -1) {
              // Extract everything from the '{' onward
              jsonPart = c.substring(startIndex);

            } else { //error if not connected to server
              Serial.println("No game.");
              while (true)
                ;
            }
            gameID = getID(jsonPart); //get the game ID, used in future requests
            if (gameID == "") { //game error , should never happen unless issue on server end
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Request Failed: ");
              lcd.setCursor(0, 1);
              lcd.print(jsonPart);
              lcd.setCursor(0, 2);
              lcd.print(jsonPart.substring(20));
            }
            userColor = getColor(jsonPart); //get the user color for the game
            Serial.println(gameID); //debugging
            Serial.println(userColor);
            Serial.print("\n\n\n\n\n");
            delay(700);
            String dec = ""; //decision (from the opponent)
            while (dec == "") { //read until a decision made, accepted or declined etc
              delay(300);
              String t = read_JSON();
              if (t.length() != 0) { //decision made

                lcd.clear();
                lcd.setCursor(0, 0);
                t = t.substring(t.indexOf('{')); //extract JSON
                Serial.println(t);

                dec = getDecision(t);
                if (dec != "accepted") { //challenge not accepted, notify user and return to game selection sequence
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Challenge not");
                  lcd.setCursor(0, 1);
                  lcd.print("Accepted...");
                  lcd.setCursor(0, 2);
                  lcd.print("Press White");
                  lcd.setCursor(0, 3);
                  lcd.print("to Return");
                  userColor = "";
                  gameID = "";
                  userColorInt = 0; //reset all parameters
                  while(digitalRead(2)==HIGH); //wait until pressed
                } else { //game accepted
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Challenge Accepted:");
                  lcd.setCursor(0, 1);
                  lcd.print("You have the");
                  lcd.setCursor(0, 2);
                  lcd.print(userColor + " pieces!"); //color of pieces for user
                  lcd.setCursor(0, 3);
                  lcd.print("Press to continue");
                  delay(1000);
                  if (userColor == "white") { //setup for colors for the game
                    userColorInt = 2; //2 because that is the pin for white button
                    userMove = 1; //user white - user moves first
                    otherColor = 3; //3 because that is the pin for the black button
                  }

                  else { //user is black
                    userColorInt = 3;
                    userMove = 0; //user black - opponent moves first
                    otherColor = 2;
                  }
                  client.stop(); 

                  diagonalNum = 1; //used for recalibration throughout
                  //do all initialization tasks
                  initializeChessBoard();
                  printBoard();
                  gameStarted = 1;
                  emState = 0;
                  //debugging
                  Serial.println(userColor);
                  Serial.println(userColorInt);
                  Serial.println(userMove);
                }
              }
            }
          }
          if (gamemodeIndex == 1) { //if bot

            Post = Post + "&level=6&color=white"; //level 6 bot 
            postAPIRequest("/api/challenge/ai", Post); //send api request
            delay(500);
            String c = read_JSON();
            String jsonPart = "";
            int startIndex = c.indexOf('{'); //extract JSON

            jsonPart = c.substring(startIndex);

            Serial.println(jsonPart);
            gameID = getID(jsonPart);
            if (gameID == "") {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Request Failed: ");
              lcd.setCursor(0, 1);
              lcd.print(jsonPart);
              lcd.setCursor(0, 2);
              lcd.print(jsonPart.substring(20));
            }
            userColor = getColorAI(jsonPart); //get user color for AI games
            //debugging
            Serial.println(gameID);
            Serial.println(userColor);
            Serial.print("\n\n\n\n\n");
            delay(700);
            //tell user info about the game
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Challenge Accepted:");
            lcd.setCursor(0, 1);
            lcd.print("You have the");
            lcd.setCursor(0, 2);
            lcd.print(userColor + " pieces!");
            lcd.setCursor(0, 3);
            lcd.print("Press to continue");
            delay(1000);
            if (userColor == "white") { //setup color/move scheme
              userColorInt = 2;
              userMove = 1;
              otherColor = 3;
            }

            else {
              userColorInt = 3;
              userMove = 0;
              otherColor = 2;
            }
            client.stop();

            diagonalNum = 1;
            //various initializations to begin game
            initializeChessBoard();
            printBoard();
            gameStarted = 1;
            emState = 0;
            Serial.println(userColor);
            Serial.println(userColorInt);
            Serial.println(userMove);
          }

          //reset all game selection variables so that once the user has returned
          //they are able to freely select and start another game without issue
          timeIndex = friendIndex = gamemodeIndex = 0;
          selectingFriends = selectingTime = 0;
          onetime1 = onetime2 = onetime3 = 0;
          //initialize promotion, not used often
          promotion = 0;
          promotionPiece = ' ';
          
          while (digitalRead(userColorInt) == HIGH) //wait for press of user's button
            ;

          delay(400);


          selectingGamemode = 1; //when user returns to game selection after game, will have to select game mode again
          lcd.clear();

        }  //end button pressed loop for starting a game
      }
    }




  }  //end gameStarted loop
  if (gameStarted == 1) { //during a game
   
    if (userMove == 0) { //opponent's move
      if(wrong==0) {
      lastBoardMove = move; //used for illegal move detection
      }
      if (once == 0) {
        makeAPIRequest("/api/board/game/stream/" + gameID);  //read for new move to be made
        delay(500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Waiting for");
        lcd.setCursor(0, 1);
        lcd.print("Opponent's Move");

        
        move = lastMove;
        once = 1;
        //debugging
        Serial.println(move);
        Serial.println(lastMove);
      }
      
      unsigned long startingTime = millis(); //used to occasionally reconnect to server, as if a move is made too quickly in a specific window,
      //it will not be detected and if we did not reconnect occasionally, that move would never be read
      //effectively locking the program in this state forever
       
      while (move == lastMove || move=="") { //while  nothing read yet - no move made
        if ((millis() - startingTime) > 5000) { //after 5s, reconnect to server and read again for reasons stated above
          startingTime = millis();
          client.stop();
          makeAPIRequest("/api/board/game/stream/" + gameID);  //read for new move to be made
          delay(500);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Waiting for");
          lcd.setCursor(0, 1);
          lcd.print("Opponent's Move");
        }
        resp = read_JSON(); //keep reading 
        delay(200);
       
        Serial.println(resp);
        
        resp.trim();
        if (resp.length() > 5 && resp != "1") { //if length greater than few chars, JSON response received
        //move received
          //debugging
          Serial.println("\n\n\n Most Recent Move: ");
          Serial.println(resp);
          if (getRecentMove(resp) != "N/A") { //read move
            //response could have been that game has ended, find the status as well as the move
            move = getRecentMove(resp);
            gameStatus = getStatus(resp);


            //if the game status is not "started", then the game is no longer ongoing, has ended
            //if mate or stalemate however, a move was made to finish the game and put the user in stalemate or checkmate, so before printing that the game is over,
            //the move that leads to this ending needs to happen first to show how the user lost or drew and what the opponent did 
            //but otherwise if it is just a draw or resignation, no move was made and the opponent simply ended the game on their own
            if (gameStatus != "started" && gameStatus!="mate" && gameStatus!= "stalemate") { //game is OVER
              gameStarted = 0; //indicate no game ongoing anymore
              //print to user on LCD that game has ended and the result
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("GAME ENDED!");
              lcd.setCursor(0, 1);
              lcd.print(gameStatus);
              lcd.setCursor(0, 2);
              //debugging
              Serial.println("HERE IS GAME STATUS!!!");
              Serial.println(gameStatus);
              if (gameStatus == "stalemate" || gameStatus == "draw") { //nobody wins
                lcd.print("No winner");
              } else { //winner
                lcd.print("Winner " + getWinner(resp));
              }
              lcd.setCursor(0, 3);
              lcd.print("Press to continue");
              client.stop();
              once = 0;
              while (digitalRead(2) == HIGH && digitalRead(3) == HIGH) //wait until press
                ;
              delay(300);
              break;
            }
         //debugging
            Serial.println(move);
          }
        }
      }
      //debugging
Serial.print("MOVE, LASTBOARDMOVE " + move + " " + lastBoardMove);

      if (move != lastMove && gameStarted == 1 && move!=lastBoardMove && move!="") { //move made, game not ended
      //if move was = lastBoardMove during a bot game that would mean that the user had made an illegal move
      
        copyBoard(0); //copy board state to previous
        wrong=0;
        //notify user
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("MOVE MADE");
        lcd.setCursor(0, 1);
        lcd.print(move);
        //if length == 5, promotion, so
        //d4d5 is no promotion but if d7d8q, means that there was queen promotion, and length for those moves is 5
        if (move.length() == 5) {
          lcd.setCursor(0, 2);
          lcd.print("Promoted to " + move.charAt(4)); //what did they promote to 
          promotion = 1;
          promotionPiece = toupper(move.charAt(4)); //for board state
        }
        int file1 = move.charAt(0) - 'a' + 1;
        int file2 = move.charAt(2) - 'a' + 1;
        int rank1 = move.charAt(1) - 'a' + 1;
        int rank2 = move.charAt(3) - 'a' + 1;

        int rankMovement = file2 - file1;
        int fileMovement = rank2 - rank1;
        //debugging
        Serial.println("Movement needed: ");
        Serial.print("file: ");
        Serial.print(fileMovement);
        Serial.print(" rank: ");
        Serial.print(rankMovement);
        //home square file and rank
        int lastfile = lastSquare.charAt(0) - 'a' + 1;

        int lastrank = lastSquare.charAt(1) - 'a' + 1;
        int d5file = 'd' - 'a' + 1;
        int d5rank = '5' - 'a' + 1;

        client.stop();

        //find rows and cols in array from the squares from move made
        int row = '8' - move.charAt(3);
        int rowFrom = '8' - move.charAt(1);
        int col = move.charAt(2) - 'a';
        int colFrom = move.charAt(0) - 'a';
        Serial.println(String(row) + String(rowFrom) + String(col) + String(colFrom));

        //if king moves more than a square, it is castling
        if (board[rowFrom][colFrom].type == 'K' && (abs(col - colFrom) > 1)) castling(move);

        //if a pawn is going diagonally to a square with no piece, it is en passant
        else {
          if (board[rowFrom][colFrom].type == 'P' && board[row][col].type == ' ' && fileMovement!=0 && rankMovement!=0) {
            enPassant(move);
          }

          else { //no special move
            if ((board[row][col].color == userColorInt)) {  //capture, the destination square is currently occupied by a user's piece
              //capturing sequence

              //update board state
              board[row][col].color = otherColor;
              board[row][col].type = board[rowFrom][colFrom].type;

              if (promotion == 1) { //if promotion while capturing
                board[row][col].type = promotionPiece;
                promotion = 0;
                promotionPiece = ' ';
              }

              //move firstly to the user's piece to take it off the board 
              moveRank(file2 - lastfile, 0);
              //delay(200);
              moveFile(rank2 - lastrank, 0);
              //delay(200);

              //move to the edge of the square to the left
              moveRank(-0.6, 1);
              //delay(200);

              //move up to the middle of the board at the edge of the square below the fifth rank
              //this is the middle line of the board and where the piece will be taken off 
              moveFile(d5rank - rank2 - 0.4, 1);
              //delay(200);

              //move entirely off of the board and into the space to the side where pieces are captured
              moveRank(-file2, 1);

              //it should be outside of board to indicate capture
              //now move to the moving piece's square
              emOff(); //turn off EM
              delay(200);
              //move from the outside of the board to the rank of the opponent's piece that moved 
              moveRank(file1 + 0.6, 0);
              //delay(200);

              //move to the file of the opponent's piece, should now be positioned beneath the opponent's piece
              //movement of this piece is handled after this logic
              moveFile(0.4 + rank1 - d5rank, 0);
              // delay(200);

            } else {  //no piece there, no capture
            //update board state
              board[row][col].color = otherColor;
              board[row][col].type = board[rowFrom][colFrom].type;
              if (promotion == 1) { //if promotion
                board[row][col].type = promotionPiece;
                promotion = 0;
                promotionPiece = ' ';
              }
              //move to the square of the piece that is moving
              moveRank(file1 - lastfile, 0);
              //delay(200);
              moveFile(rank1 - lastrank, 0);
              //delay(200);


            }

          }  //end en passant else
          //where the piece comes from is now unoccupied, set that in board state
          board[rowFrom][colFrom].color = 0;
          board[rowFrom][colFrom].type = ' ';

          if (fileMovement != 0 && rankMovement != 0) { //both movements are nonzero, indicating that movement is NOT in a straight line
            //MOVE EITHER ON EDGES OF SQUARES OR DIAGONAL
            //find abs values of the movements to see whether it is a knight move or a diagonal
            int absfile = abs(fileMovement);
            int absrank = abs(rankMovement);

            if (absfile != absrank) { //if they are unequal - knight move , bc knight moves 2 squares one way, 1 square another way
              //KNIGHT MOVE - MOVE ON EDGE OF SQUARES
              //go to edge of square with the knight 
              moveRank(-0.6, 1);
              //delay(200);

              //move file
              if (rank2 == 1) moveFile(fileMovement + 0.6, 1); //if the rank is at the bottom, move upwards instead of downwards so it has room
              else moveFile(fileMovement - 0.4, 1);
              //delay(200);

              //move the rank of the knight movement, back to center of the square in that direction
              moveRank(rankMovement + 0.6, 1);
              //delay(200);

              //move back to center of the square
              if (rank2 == 1) moveFile(-0.6, 1);
              else moveFile(0.4, 1);
              //delay(200);



            } else {
              //MOVE DIAGONALLY - BISHOP/QUEEN DIAGONAL OR PAWN TAKING
              //movements are EQUAL 

              //logic below determines which diagonal to move on based on the file and rank movements along with moveDiag and moveDiag2
              if (fileMovement > 0) { //down
                if (rankMovement > 0) { //right
                  moveDiag(-abs(fileMovement), 1);
                  //delay(200);
                } else { //left
                  moveDiag2(-abs(fileMovement), 1);
                  //delay(200);
                }
              } 
              
              else { //up
                if (rankMovement > 0) { //right
                  moveDiag2(abs(fileMovement), 1);
                  //delay(200);
                } else { //left
                  moveDiag(abs(fileMovement), 1);
                  //delay(200);
                }
              }
            }
          }

          else { //otherwise straight movement

            moveFile(fileMovement, 1);
            // delay(200);
            moveRank(rankMovement, 1);
            //delay(200);
          }
        }  // end castling else
        //update detection
        fillPrevArray();
        fillArray();

        //turn off EM
        emOff();
        delay(200);

        //move back to the d5 home square with EM turned off
        moveRank(d5file - file2, 0);
        // delay(200);
        moveFile(d5rank - rank2, 0);
        //delay(200);

        //below is used for recalibration
        if (diagonalNum % 3 == 0) {
          //every few diagonal moves, recalibrate the system
          //this is done because diagonals are not as accurate as straight line movements, and can cause some inaccuracy within the system
          //movements need to be on squares very closely, so recalibration helps amend these inaccuracies

          //NOTIFY THE USER TO NOT PRESS BUTTONS DURING RECALIBRATION
          //this is because these pins are also used for the limit switches
          //if a user presses the buttons, the rail system will think a limit switch was hit
          //this will result in the electromagnet not knowing where it is
          //so no pieces will be able to be moved with any accuracy
          //and the rail system will likely be grinding against an edge because of this as well
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Recalibrating...");
          lcd.setCursor(0, 1);
          lcd.print("Please do not");
          lcd.setCursor(0, 2);
          lcd.print("press buttons!");
          

          emOff();
          //recalibrate
          goToD5();

          diagonalNum++;
        } //end recalibration

        if (squares[row][col] == 1) { 
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Adjust piece");
        }
       


        lastSquare = "d5";

        //now it is the user's turn
        userMove = 1;

        once = 0;
        copyBoard(0);
        
        //debugging
        printBoard(); //print the board state and last board
        Serial.println("LAST BOARD");
        printLastBoard();

        if(gameStatus!= "started" && (gameStatus=="mate" || gameStatus == "stalemate")) { //now if the game ended in a mate, display to user and end the game
          gameStarted = 0; userMove=0;
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("GAME ENDED!");
              lcd.setCursor(0, 1);
              lcd.print(gameStatus);
              lcd.setCursor(0, 2);
              
              if (gameStatus == "stalemate" || gameStatus == "draw") {

                lcd.print("No winner");
              } else {
                lcd.print("Winner " + getWinner(resp));
              }
              lcd.setCursor(0, 3);
              lcd.print("Press to continue");
              client.stop();
              once = 0;
              while (digitalRead(2) == HIGH && digitalRead(3) == HIGH)
                ;
              delay(300);
              
        } //end mate loop
      }  //end movement loop
      else if(move == lastBoardMove) { //illegal move made during a bot game
        Serial.println("LASTBOARD");
        printLastBoard();
        wrong=1;
        copyBoard(1); //REVERT board state
        userMove=1; //back to user's move, as they made an illegal move
        once=0;

        printBoard();

        //tell user what they did and to fix
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("ILLEGAL MOVE: " + lastMove);
            lcd.setCursor(0, 1);
            lcd.print("Move piece(s) back.");
            lcd.setCursor(0, 2);
            lcd.print("Press " + userColor);
            lcd.setCursor(0, 3);
            lcd.print("to continue.");
            delay(1000);
            client.stop(); //stop reading from server bc no move will be sent - it is still user move
            while (digitalRead(userColorInt) == HIGH) //wait for press
              ;
      }
    }    //end userMove == 0 loop

    if (userMove == 1) { //user's move now
      //SEND A USER MOVE 
  

      boardMove(); //logic handled mainly in this function
      if (gameStarted == 1) { //if game not over
        //tell user what move they just sent
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Move Sent:");
        lcd.setCursor(0, 1);
        lcd.print(moveMade);
        printBoard(); //debugging

        postAPIRequest("/api/board/game/" + gameID + "/move/" + moveMade, ""); //send the move to the game

        lastMove = moveMade;

        delay(500);
        //get the response from the move
        String responser = read_JSON();
        Serial.println("RESP : " + responser);
        client.stop();

        if (responser.startsWith("HTTP/1.1 200") || responser.length() < 3) {
          //http 200 - good move, now turn it over to opponent move
          userMove = 0;
          delay(300);
          
        } else { //bad response, something has happened to the game state either ended or illegal move
          int jsonStart = responser.indexOf("\r\n\r\n");
          String json = responser.substring(jsonStart + 4);  // skip "\r\n\r\n"
          // Parse it
          StaticJsonDocument<200> doc;
          DeserializationError err = deserializeJson(doc, json);

          if (err) {
            Serial.print("JSON parse failed: ");
            Serial.println(err.f_str());
            while (1)
              ;
          }

          // Access the error message
          String errorMsg = String(doc["error"]);
          Serial.print("Error: ");
          Serial.println(errorMsg);
          if (errorMsg == "Not your turn, or game already over") {
            gameStarted = 0; //game ended
            makeAPIRequest("/api/board/game/stream/" + gameID);  //read for game status
            delay(500);
            String resp = read_JSON();
            delay(200);
            resp.trim();


            //get the response from the stream to read the status of the game, means game has ended 
            if (resp.length() > 5 && resp != "1") {

              if (getRecentMove(resp) != "N/A") {

                gameStatus = getStatus(resp);
                if (gameStatus != "started") {
                  gameStarted = 0;

                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("GAME ENDED!");
                  lcd.setCursor(0, 1);
                  lcd.print(gameStatus);
                  lcd.setCursor(0, 2);
                  if (gameStatus == "stalemate" || gameStatus == "draw") {

                    lcd.print("No winner");
                  } else {
                    lcd.print("Winner " + getWinner(resp));
                  }
                  lcd.setCursor(0, 3);
                  lcd.print("Press to continue");
                  client.stop();
                  once = 0;
                  while (digitalRead(2) == HIGH && digitalRead(3) == HIGH)
                    ;
                  delay(300);
                }
              }
            }
          } //end game already over
          else { //ILLEGAL MOVE MADE
            //rever the board state
            copyBoard(1);

            //notify the user of the illegal move made, tell them to make a new move
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("ILLEGAL MOVE: " + lastMove);
            lcd.setCursor(0, 1);
            lcd.print("Move piece(s) back.");
            lcd.setCursor(0, 2);
            lcd.print("Press " + userColor);
            lcd.setCursor(0, 3);
            lcd.print("to continue.");

            //wait for press
            while (digitalRead(userColorInt) == HIGH)
              ;


          }
        }  //end big else
      }    //end gamestarted==1 loop
    }      // end userMove==1 loop
  }        //end if gameStarted gameplay loop
}  //end entire loop