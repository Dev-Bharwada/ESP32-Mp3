#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <VS1053.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCI_CLOCKF 0x03
//buttons
#define PLAY_PAUSE 32
#define VOL_UP 33
#define VOL_DOWN 25
#define PREV 26
#define NEXT 27
#define SHUFFLE 23
// SPI pins (ESP32 VSPI)
#define PIN_SCK 18
#define PIN_MISO 19
#define PIN_MOSI 21

// VS1053 pins
#define VS1053_CS 5   // Control chip select
#define VS1053_DCS 16 // Data chip select
#define VS1053_DREQ 4 // Data request pin

// SD card
#define SDCARD_CS 22 // SD card chip select

//OLED
#define OLED_RESET -1
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


#define MAX_FILES 70 // Maximum number of MP3 files
#define MAX_FILENAME_LEN 100

char fileList[MAX_FILES][MAX_FILENAME_LEN];

int volume = 77; 
int currentTrack = 0;
int totalTracks = 0;
bool paused = false;
bool shuffleEnabled = false;
int lastTrack = -1;
VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
File mp3File;

//const int BUFFER_SIZE = 1024;
const int BUFFER_SIZE = 4096;
uint8_t mp3Buffer[BUFFER_SIZE];

const char* trackNames[] = {
  "Song One - Artist A",
  "Song Two - Artist B",
  "Song Three - Artist C",
  "Song Four - Artist D"
};
const int TRACK_COUNT = sizeof(trackNames) / sizeof(trackNames[0]);

void updateDisplay()
{
  display.clearDisplay();
  display.setCursor(0, 0);

  display.println("Now Playing:");
  display.println("");

  display.println(trackNames[currentTrack]);

  display.println("");
  display.print("Volume: ");
  display.print(volume);

  // Draw volume bar (0–100 scaled to 100px width)
  int barWidth = map(volume, 0, 100, 0, 100);
  display.drawRect(14, 54, 100, 8, WHITE);
  display.fillRect(14, 54, barWidth, 8, WHITE);

  display.display();
}

// List all files in the SD card root directory
void listDirectory(fs::FS &fs, const char *dirname)
{

  Serial.println("------");

  int index = 0;
  File root = fs.open(dirname);

  if (!root || !root.isDirectory())
  {
    Serial.println("Failed to open SD directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (!file.isDirectory() && index < MAX_FILES)
    {
      strncpy(fileList[index], file.name(), MAX_FILENAME_LEN - 1);
      fileList[index][MAX_FILENAME_LEN - 1] = '\0';
      index++;
    }
    file = root.openNextFile();
  }

  totalTracks = index;

  Serial.println("MP3 files found:");
  for (int i = 0; i < index; i++)
  {
    Serial.print("\t");
    Serial.println(fileList[i]);
  }

  Serial.println("------");
}

// Move to the next track
void nextTrack()
{

  if (mp3File)
  {
    mp3File.close();
  }

  if (shuffleEnabled && totalTracks > 1)
  {
    int next;
    do
    {
      next = random(0, totalTracks);
    } while (next == currentTrack); // avoid repeating same song
    currentTrack = next;
  }
  else
  {
    currentTrack++;
    if (currentTrack >= totalTracks)
    {
      currentTrack = 0;
    }
  }

  char path[500];
  snprintf(path, sizeof(path), "/%s", fileList[currentTrack]);
  mp3File = SD.open(path);

  Serial.println(path);
  updateDisplay();
}

void previousTrack()
{

  if (mp3File)
  {
    mp3File.close();
  }

  if (shuffleEnabled && totalTracks > 1)
  {
    int prev;
    do
    {
      prev = random(0, totalTracks);
    } while (prev == currentTrack);
    currentTrack = prev;
  }
  else
  {
    if (currentTrack == 0)
    {
      currentTrack = totalTracks - 1;
    }
    else
    {
      currentTrack--;
    }
  }

  char path[500];
  snprintf(path, sizeof(path), "/%s", fileList[currentTrack]);
  mp3File = SD.open(path);

  Serial.print("Playing: ");
  Serial.println(path);
  updateDisplay();
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  pinMode(PLAY_PAUSE, INPUT);
  pinMode(VOL_UP, INPUT);
  pinMode(VOL_DOWN, INPUT);
  pinMode(PREV, INPUT);
  pinMode(NEXT, INPUT);
  pinMode(SHUFFLE, INPUT);

  Serial.println();
  Serial.println("MP3 file player from SD card");

  // Disable VS1053 before SD initialization
  pinMode(VS1053_CS, OUTPUT);
  pinMode(VS1053_DCS, OUTPUT);
  digitalWrite(VS1053_CS, HIGH);
  digitalWrite(VS1053_DCS, HIGH);

  // Initialize SPI
  SPI.begin(18, 19, 21, 22);

  // SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, SDCARD_CS);

  // Mount SD card
  delay(300);
  bool sdReady = false;

for (int i = 0; i < 5; i++)
{
  if (SD.begin(SDCARD_CS, SPI))
  {
    sdReady = true;
    break;
  }
  delay(200);
}

if (!sdReady)
{
  Serial.println("SD mount failed");
  while (1);   // stop here
}

  Serial.println("SD card OK");

  listDirectory(SD, "/");
  currentTrack = 0;

char path[64];
snprintf(path, sizeof(path), "/%s", fileList[currentTrack]);
mp3File = SD.open(path);

Serial.print("Playing: ");
Serial.println(path);
  // Initialize VS1053
  player.begin();
  player.switchToMp3Mode();
  player.writeRegister(SCI_CLOCKF, 0x6000);
  delay(10);
  player.setVolume(volume);
  randomSeed(esp_random());

 
  
  Serial.println("Controls:");
  Serial.println("\tn : next track");
  Serial.println("\tp : pause / resume");
  Serial.println("\t+ / - : volume up / down");
  Serial.println("\t+ / b : previous track");
  Serial.println("\ts : toggle shuffle");
  Serial.println("------");

  Wire.begin(17, 13);  // SDA, SCL
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
  Serial.println("SSD1306 allocation failed");
  while (1);
}
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  updateDisplay();
  display.display(); 
}

void loop()
{

  bool play_pause_state = digitalRead(PLAY_PAUSE);
  static bool play_pause_lastState = LOW;
  //play pause button
    if (play_pause_state == HIGH && play_pause_lastState == LOW)
    {
      paused = !paused;
      Serial.println(paused ? "Paused" : "Playing");
    }
    play_pause_lastState = play_pause_state;

  bool vol_up_state = digitalRead(VOL_UP);
  static bool vol_up_lastState = LOW;
  //volume up button
  
    if (vol_up_state == HIGH && vol_up_lastState == LOW && volume < 100)
    {
      volume++;
      player.setVolume(volume);
      Serial.println("Volume up");
      updateDisplay();
    }
vol_up_lastState = vol_up_state;

  bool vol_down_state = digitalRead(VOL_DOWN);
  static bool vol_down_lastState = LOW;
if (vol_down_state == HIGH && vol_down_lastState == LOW && volume > 0)
      {
       volume--;
       player.setVolume(volume);
       Serial.println("Volume down");
       updateDisplay();
    }
    vol_down_lastState = vol_down_state;

  bool next_track_state = digitalRead(NEXT);
  static bool next_track_lastState = LOW;
  if (next_track_state == HIGH && next_track_lastState == LOW)
      {
       Serial.println("Next track");
       nextTrack();
    }
    next_track_lastState = next_track_state;

  bool previous_track_state = digitalRead(PREV);
  static bool previous_track_lastState = LOW;
  if (previous_track_state == HIGH && previous_track_lastState == LOW)
      {
       Serial.println("Previous track");
      previousTrack();
    }
  previous_track_lastState = previous_track_state;

  bool shuffle_state = digitalRead(SHUFFLE);
  static bool shuffle_lastState = LOW;
  if (shuffle_state == HIGH && shuffle_lastState == LOW)
      {
       shuffleEnabled = !shuffleEnabled;
       Serial.println(shuffleEnabled ? "Shuffle ON" : "Shuffle OFF");
    }
    shuffle_lastState = shuffle_state;

  if (Serial.available())
  {
    char c = Serial.read();
  
    if (c == 'p')
    {
      paused = !paused;
      Serial.println(paused ? "Paused" : "Playing");
    }
  
    if (c == '+' && volume < 100)
    {
      volume++;
      player.setVolume(volume);
      Serial.println("Volume up");
      updateDisplay();
    }
    //VOLUME DOWN

    if (c == '-' && volume > 0)
    {
      volume--;
      player.setVolume(volume);
      Serial.println("Volume down");
      updateDisplay();
    }
    //NEXT TRACK
    
    if (c == 'n')
    {
      Serial.println("Next track");
      nextTrack();
    }
    //PREVIOUS TRACK
    
    if (c == 'b')
    { // b = back 
      Serial.println("Previous track");
      previousTrack();
    }
    //SHUFFLE
  
    if (c == 's')
    {
      shuffleEnabled = !shuffleEnabled;
      Serial.println(shuffleEnabled ? "Shuffle ON" : "Shuffle OFF");
    }
  }

  if (!paused)
  {
    int bytesRead = 0;

    if (mp3File)
    {
      bytesRead = mp3File.read(mp3Buffer, BUFFER_SIZE);
    }

    if (bytesRead)
    {
      while (!digitalRead(VS1053_DREQ))
      {
        // wait until VS1053 can accept data
        yield(); // important on ESP32
      }
      player.playChunk(mp3Buffer, bytesRead);
    }
    else
    {
      nextTrack();
    }
  }
}