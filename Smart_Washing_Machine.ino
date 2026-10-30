// ======================================================
// DIGITECH DYNAMO
// ======================================================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// ======================================================
// OLED
// ======================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define OLED_ADDR 0x3C

// ======================================================
// INPUT PINS
// ======================================================

#define PIN_ENTER 22
#define PIN_BACK  23
#define PIN_UP    24
#define PIN_DOWN  25

#define PIN_LID   26

#define PIN_BUZZER 27

// ======================================================
// OUTPUT PINS
// ======================================================

#define PIN_WATER 28
#define PIN_WASH  29
#define PIN_DRAIN 30
#define PIN_SPIN  31

#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// ======================================================
// EEPROM
// ======================================================

#define EEPROM_MAGIC 0x55AA

struct MachineSettings {

  uint16_t magic;

  byte washTime;
  byte spinTime;

  byte waterLevel;

  byte washMode;

  byte delayHours;
  byte delayMinutes;

  byte childLock;

  byte rinseCount;
};

MachineSettings cfg;

// ======================================================
// WASH MODES
// ======================================================

#define MODE_HARD   0
#define MODE_SOFT   1
#define MODE_DRAIN  2

// ======================================================
// PROGRAM TYPES
// ======================================================

#define PROGRAM_WASH 0
#define PROGRAM_SPIN 1

byte programType = PROGRAM_WASH;

// ======================================================
// MACHINE STATES
// ======================================================

enum MachineState {

  ST_IDLE,

  ST_DELAY,

  ST_FILL,

  ST_WASH,

  ST_DRAIN_1,

  ST_RINSE_FILL,

  ST_RINSE,

  ST_DRAIN_2,

  ST_SPIN,

  ST_PAUSE,

  ST_COMPLETE,

  ST_ERROR

};

MachineState state = ST_IDLE;

// ======================================================
// WASH MOTOR PHASE
// ======================================================

#define MOTOR_FORWARD 0
#define MOTOR_REVERSE 1

byte motorDirection = MOTOR_FORWARD;

unsigned long motorPhaseStart = 0;

unsigned long motorPhaseTime = 0;

// ======================================================
// TIMERS
// ======================================================

unsigned long stateStart = 0;

unsigned long stateDuration = 0;

unsigned long pauseRemaining = 0;

unsigned long delayStart = 0;

unsigned long delayDuration = 0;

// ======================================================
// VARIABLES
// ======================================================

bool welcomeScreen = true;

int mainMenu = 0;

int selectedProgram = 0;

int selectedWater = 40;

int selectedMode = MODE_SOFT;

int manualMinutes = 10;

int selectedSpin = 5;

int selectedRinse = 1;

bool errorShown = false;

// ======================================================
// BUTTON DEBOUNCE
// ======================================================

struct Button {

  byte pin;

  bool last;

  unsigned long downTime;

};

Button btnEnter = {PIN_ENTER, HIGH, 0};
Button btnBack  = {PIN_BACK, HIGH, 0};
Button btnUp    = {PIN_UP, HIGH, 0};
Button btnDown  = {PIN_DOWN, HIGH, 0};

// ======================================================
// FUNCTION DECLARATIONS
// ======================================================

bool pressed(Button &b);

bool longPressed(Button &b);

void startupScreen();

void welcome();

void mainMenuScreen();

void programMenu();

void waterMenu();

void modeMenu();

void delayMenu();

void settingsMenu();

void startSelectedProgram();

void startWash();

void startSpin();

void startFill();

void startRinse();

void updateMachine();

void updateFill();

void updateWash();

void updateDrain();

void updateRinseFill();

void updateRinse();

void updateSpin();

void updateDelay();

void updatePause();

void motorControl();

void stopEverything();

void pauseMachine();

void resumeMachine();

void completeMachine();

void lidSafety();

void errorScreen(const char *code, const char *text);

void saveSettings();

void loadSettings();

void beep();

void beepOK();

void beepError();

void completeBeep();

void showStatus(const char *title);

void formatTime(unsigned long ms, char *buffer);

void resetSettings();

// ======================================================
// SETUP
// ======================================================

void setup() {

  pinMode(PIN_ENTER, INPUT_PULLUP);
  pinMode(PIN_BACK, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);

  pinMode(PIN_LID, INPUT_PULLUP);

  pinMode(PIN_BUZZER, OUTPUT);

  pinMode(PIN_WATER, OUTPUT);
  pinMode(PIN_WASH, OUTPUT);
  pinMode(PIN_DRAIN, OUTPUT);
  pinMode(PIN_SPIN, OUTPUT);

  stopEverything();

  Serial.begin(9600);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {

    while (1) {
      // OLED ERROR
    }
  }

  oled.clearDisplay();
  oled.display();

  loadSettings();

  startupScreen();

  delay(1800);

  welcome();
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  // ------------------------------------------
  // LID SAFETY
  // ------------------------------------------

  lidSafety();

  // ------------------------------------------
  // CHILD LOCK
  // ------------------------------------------

  if (cfg.childLock == 1) {

    oled.clearDisplay();

    oled.setTextColor(SSD1306_WHITE);

    oled.setTextSize(2);

    oled.setCursor(18, 5);
    oled.println("LOCKED");

    oled.setTextSize(1);

    oled.setCursor(15, 35);
    oled.println("ENTER 3 SEC");

    oled.setCursor(15, 50);
    oled.println("TO UNLOCK");

    oled.display();

    if (longPressed(btnEnter)) {

      cfg.childLock = 0;

      saveSettings();

      beepOK();

      delay(700);

      mainMenuScreen();
    }

    return;
  }

  // ------------------------------------------
  // RUNNING
  // ------------------------------------------

  if (state != ST_IDLE &&
      state != ST_COMPLETE &&
      state != ST_ERROR) {

    updateMachine();

    return;
  }

  // ------------------------------------------
  // WELCOME
  // ------------------------------------------

  if (welcomeScreen) {

    if (pressed(btnEnter)) {

      welcomeScreen = false;

      beep();

      delay(100);

      mainMenuScreen();
    }

    return;
  }

  // ------------------------------------------
  // COMPLETE
  // ------------------------------------------

  if (state == ST_COMPLETE) {

    if (pressed(btnEnter) || pressed(btnBack)) {

      state = ST_IDLE;

      mainMenuScreen();
    }

    return;
  }

  // ------------------------------------------
  // MAIN MENU
  // ------------------------------------------

  mainMenuScreen();

  if (pressed(btnUp)) {

    mainMenu--;

    if (mainMenu < 0)
      mainMenu = 5;

    beep();
  }

  if (pressed(btnDown)) {

    mainMenu++;

    if (mainMenu > 5)
      mainMenu = 0;

    beep();
  }

  if (pressed(btnEnter)) {

    beepOK();

    switch (mainMenu) {

      case 0:
        programMenu();
        break;

      case 1:
        waterMenu();
        break;

      case 2:
        modeMenu();
        break;

      case 3:
        delayMenu();
        break;

      case 4:
        settingsMenu();
        break;

      case 5:
        startSelectedProgram();
        break;
    }
  }
}

// ======================================================
// STARTUP
// ======================================================

void startupScreen() {

  oled.clearDisplay();

  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(2);

  oled.setCursor(8, 5);
  oled.println("DIGITECH");

  oled.setCursor(18, 27);
  oled.println("DYNAMO");

  oled.setTextSize(1);

  oled.setCursor(28, 51);
  oled.println("SMART WASHER");

  oled.display();
}

// ======================================================
// WELCOME
// ======================================================

void welcome() {

  welcomeScreen = true;

  oled.clearDisplay();

  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(1);

  oled.setCursor(43, 0);
  oled.println("WELCOME");

  oled.setTextSize(2);

  oled.setCursor(10, 15);
  oled.println("SMART");

  oled.setCursor(22, 34);
  oled.println("WASHER");

  oled.setTextSize(1);

  oled.setCursor(25, 56);
  oled.println("PRESS ENTER");

  oled.display();
}

// ======================================================
// MAIN MENU
// ======================================================

void mainMenuScreen() {

  oled.clearDisplay();

  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(1);

  oled.setCursor(0, 0);
  oled.println("==== MAIN MENU ====");

  const char *items[] = {

    "Program",
    "Water Level",
    "Wash Mode",
    "Delay Start",
    "Settings",
    "START PROGRAM"
  };

  for (int i = 0; i < 6; i++) {

    oled.setCursor(0, 10 + i * 9);

    if (i == mainMenu)
      oled.print("> ");
    else
      oled.print("  ");

    oled.println(items[i]);
  }

  oled.display();
}

// ======================================================
// PROGRAM MENU
// ======================================================

void programMenu() {

  const char *items[] = {

    "5 MIN WASH",
    "15 MIN WASH",
    "30 MIN WASH",
    "45 MIN WASH",
    "MANUAL WASH",
    "SPIN ONLY"
  };

  int index = selectedProgram;

  while (true) {

    oled.clearDisplay();

    oled.setTextSize(1);

    oled.setCursor(25, 0);
    oled.println("PROGRAM");

    for (int i = 0; i < 6; i++) {

      oled.setCursor(0, 10 + i * 9);

      if (i == index)
        oled.print("> ");
      else
        oled.print("  ");

      oled.println(items[i]);
    }

    oled.display();

    if (pressed(btnUp)) {

      index--;

      if (index < 0)
        index = 5;

      beep();
    }

    if (pressed(btnDown)) {

      index++;

      if (index > 5)
        index = 0;

      beep();
    }

    if (pressed(btnBack))
      return;

    if (pressed(btnEnter)) {

      selectedProgram = index;

      // --------------------------------
      // FIXED WASH
      // --------------------------------

      if (index == 0) {

        cfg.washTime = 5;

        programType = PROGRAM_WASH;

        saveSettings();

        startSelectedProgram();

        return;
      }

      if (index == 1) {

        cfg.washTime = 15;

        programType = PROGRAM_WASH;

        saveSettings();

        startSelectedProgram();

        return;
      }

      if (index == 2) {

        cfg.washTime = 30;

        programType = PROGRAM_WASH;

        saveSettings();

        startSelectedProgram();

        return;
      }

      if (index == 3) {

        cfg.washTime = 45;

        programType = PROGRAM_WASH;

        saveSettings();

        startSelectedProgram();

        return;
      }

      // --------------------------------
      // MANUAL
      // --------------------------------

      if (index == 4) {

        manualMinutes = cfg.washTime;

        if (manualMinutes < 1)
          manualMinutes = 1;

        while (true) {

          oled.clearDisplay();

          oled.setTextSize(1);

          oled.setCursor(20, 0);
          oled.println("MANUAL TIMER");

          oled.setTextSize(3);

          oled.setCursor(35, 18);

          oled.print(manualMinutes);

          oled.setTextSize(1);

          oled.setCursor(35, 47);
          oled.println("MINUTES");

          oled.setCursor(0, 57);
          oled.println("UP/DOWN  ENTER=OK");

          oled.display();

          if (pressed(btnUp)) {

            manualMinutes++;

            if (manualMinutes > 120)
              manualMinutes = 120;

            beep();
          }

          if (pressed(btnDown)) {

            manualMinutes--;

            if (manualMinutes < 1)
              manualMinutes = 1;

            beep();
          }

          if (pressed(btnBack))
            return;

          if (pressed(btnEnter)) {

            cfg.washTime = manualMinutes;

            programType = PROGRAM_WASH;

            saveSettings();

            beepOK();

            startSelectedProgram();

            return;
          }
        }
      }

      // --------------------------------
      // SPIN ONLY
      // --------------------------------

      if (index == 5) {

        const int spinOptions[] = {
          5,
          10,
          15,
          30,
          50
        };

        int spinIndex = 0;

        while (true) {

          oled.clearDisplay();

          oled.setTextSize(1);

          oled.setCursor(30, 0);
          oled.println("SPIN ONLY");

          for (int i = 0; i < 5; i++) {

            oled.setCursor(10, 12 + i * 9);

            if (i == spinIndex)
              oled.print("> ");
            else
              oled.print("  ");

            oled.print(spinOptions[i]);
            oled.println(" MIN");
          }

          oled.display();

          if (pressed(btnUp)) {

            spinIndex--;

            if (spinIndex < 0)
              spinIndex = 4;

            beep();
          }

          if (pressed(btnDown)) {

            spinIndex++;

            if (spinIndex > 4)
              spinIndex = 0;

            beep();
          }

          if (pressed(btnBack))
            return;

          if (pressed(btnEnter)) {

            cfg.spinTime =
              spinOptions[spinIndex];

            programType = PROGRAM_SPIN;

            saveSettings();

            beepOK();

            startSelectedProgram();

            return;
          }
        }
      }
    }
  }
}

// ======================================================
// WATER MENU
// ======================================================

void waterMenu() {

  const int levels[] = {
    5, 10, 20, 30, 40, 50, 60, 70, 80
  };

  int index = 4;

  for (int i = 0; i < 9; i++) {

    if (levels[i] == cfg.waterLevel)
      index = i;
  }

  while (true) {

    oled.clearDisplay();

    oled.setTextSize(1);

    oled.setCursor(25, 0);
    oled.println("WATER LEVEL");

    oled.setTextSize(3);

    oled.setCursor(38, 18);

    oled.print(levels[index]);

    oled.print("%");

    oled.setTextSize(1);

    oled.setCursor(10, 50);
    oled.println("UP/DOWN CHANGE");

    oled.setCursor(15, 59);
    oled.println("ENTER=SAVE");

    oled.display();

    if (pressed(btnUp)) {

      index--;

      if (index < 0)
        index = 8;

      beep();
    }

    if (pressed(btnDown)) {

      index++;

      if (index > 8)
        index = 0;

      beep();
    }

    if (pressed(btnBack))
      return;

    if (pressed(btnEnter)) {

      cfg.waterLevel = levels[index];

      saveSettings();

      beepOK();

      return;
    }
  }
}

// ======================================================
// MODE MENU
// ======================================================

void modeMenu() {

  const char *modes[] = {

    "HARD WASH",
    "SOFT WASH",
    "DRAIN ONLY"
  };

  int index = cfg.washMode;

  while (true) {

    oled.clearDisplay();

    oled.setTextSize(1);

    oled.setCursor(30, 0);
    oled.println("WASH MODE");

    for (int i = 0; i < 3; i++) {

      oled.setCursor(15, 15 + i * 12);

      if (i == index)
        oled.print("> ");
      else
        oled.print("  ");

      oled.println(modes[i]);
    }

    oled.display();

    if (pressed(btnUp)) {

      index--;

      if (index < 0)
        index = 2;

      beep();
    }

    if (pressed(btnDown)) {

      index++;

      if (index > 2)
        index = 0;

      beep();
    }

    if (pressed(btnBack))
      return;

    if (pressed(btnEnter)) {

      cfg.washMode = index;

      saveSettings();

      beepOK();

      return;
    }
  }
}

// ======================================================
// DELAY MENU
// ======================================================

void delayMenu() {

  const int delayValues[] = {
    0,
    30,
    60,
    120,
    240,
    480
  };

  const char *names[] = {

    "OFF",
    "30 MIN",
    "1 HOUR",
    "2 HOURS",
    "4 HOURS",
    "8 HOURS"
  };

  int current =
    cfg.delayHours * 60 +
    cfg.delayMinutes;

  int index = 0;

  for (int i = 0; i < 6; i++) {

    if (delayValues[i] == current)
      index = i;
  }

  while (true) {

    oled.clearDisplay();

    oled.setTextSize(1);

    oled.setCursor(28, 0);
    oled.println("DELAY START");

    for (int i = 0; i < 6; i++) {

      oled.setCursor(15, 10 + i * 9);

      if (i == index)
        oled.print("> ");
      else
        oled.print("  ");

      oled.println(names[i]);
    }

    oled.display();

    if (pressed(btnUp)) {

      index--;

      if (index < 0)
        index = 5;

      beep();
    }

    if (pressed(btnDown)) {

      index++;

      if (index > 5)
        index = 0;

      beep();
    }

    if (pressed(btnBack))
      return;

    if (pressed(btnEnter)) {

      int total =
        delayValues[index];

      cfg.delayHours =
        total / 60;

      cfg.delayMinutes =
        total % 60;

      saveSettings();

      beepOK();

      return;
    }
  }
}

// ======================================================
// SETTINGS MENU
// ======================================================

void settingsMenu() {

  const char *items[] = {

    "CHILD LOCK",
    "RINSE COUNT",
    "RESET MEMORY"
  };

  int index = 0;

  while (true) {

    oled.clearDisplay();

    oled.setTextSize(1);

    oled.setCursor(28, 0);
    oled.println("SETTINGS");

    for (int i = 0; i < 3; i++) {

      oled.setCursor(10, 14 + i * 12);

      if (i == index)
        oled.print("> ");
      else
        oled.print("  ");

      oled.println(items[i]);
    }

    oled.display();

    if (pressed(btnUp)) {

      index--;

      if (index < 0)
        index = 2;

      beep();
    }

    if (pressed(btnDown)) {

      index++;

      if (index > 2)
        index = 0;

      beep();
    }

    if (pressed(btnBack))
      return;

    if (pressed(btnEnter)) {

      // --------------------------------
      // CHILD LOCK
      // --------------------------------

      if (index == 0) {

        oled.clearDisplay();

        oled.setTextSize(1);

        oled.setCursor(20, 15);
        oled.println("CHILD LOCK");

        oled.setCursor(10, 35);
        oled.println("ENTER 3 SEC");

        oled.display();

        while (digitalRead(PIN_ENTER) == HIGH) {

          if (longPressed(btnEnter))
            break;
        }

        cfg.childLock = 1;

        saveSettings();

        beepOK();

        return;
      }

      // --------------------------------
      // RINSE COUNT
      // --------------------------------

      if (index == 1) {

        int count = cfg.rinseCount;

        while (true) {

          oled.clearDisplay();

          oled.setTextSize(1);

          oled.setCursor(28, 0);
          oled.println("RINSE COUNT");

          oled.setTextSize(3);

          oled.setCursor(50, 18);
          oled.print(count);

          oled.setTextSize(1);

          oled.setCursor(10, 52);
          oled.println("1-3 RINSES");

          oled.display();

          if (pressed(btnUp)) {

            count++;

            if (count > 3)
              count = 3;

            beep();
          }

          if (pressed(btnDown)) {

            count--;

            if (count < 1)
              count = 1;

            beep();
          }

          if (pressed(btnBack))
            return;

          if (pressed(btnEnter)) {

            cfg.rinseCount = count;

            saveSettings();

            beepOK();

            return;
          }
        }
      }

      // --------------------------------
      // RESET MEMORY
      // --------------------------------

      if (index == 2) {

        oled.clearDisplay();

        oled.setTextSize(1);

        oled.setCursor(15, 15);
        oled.println("RESET EEPROM?");

        oled.setCursor(10, 35);
        oled.println("ENTER=YES");

        oled.setCursor(10, 50);
        oled.println("BACK=NO");

        oled.display();

        while (true) {

          if (pressed(btnBack))
            return;

          if (pressed(btnEnter)) {

            resetSettings();

            beepOK();

            delay(700);

            return;
          }
        }
      }
    }
  }
}

// ======================================================
// START SELECTED PROGRAM
// ======================================================

void startSelectedProgram() {

  if (digitalRead(PIN_LID) == HIGH) {

    errorScreen("E01", "LID OPEN");

    return;
  }

  if (cfg.delayHours != 0 ||
      cfg.delayMinutes != 0) {

    delayDuration =
      ((unsigned long)cfg.delayHours * 60UL +
       cfg.delayMinutes) *
      60000UL;

    delayStart = millis();

    state = ST_DELAY;

    stopEverything();

    return;
  }

  if (programType == PROGRAM_SPIN) {

    startSpin();

  } else {

    startWash();
  }
}

// ======================================================
// START WASH
// ======================================================

void startWash() {

  if (digitalRead(PIN_LID) == HIGH) {

    errorScreen("E01", "LID OPEN");

    return;
  }

  if (cfg.washMode == MODE_DRAIN) {

    stateStart = millis();

    stateDuration =
      5UL * 60000UL;

    state = ST_DRAIN_1;

    stopEverything();

    digitalWrite(PIN_DRAIN, RELAY_ON);

    return;
  }

  startFill();
}

// ======================================================
// START FILL
// ======================================================

void startFill() {

  stateStart = millis();

  // Prototype fill calculation.
  // Real machine should use a water-level sensor.

  stateDuration =
    map(
      cfg.waterLevel,
      5,
      80,
      15000,
      240000
    );

  state = ST_FILL;

  stopEverything();

  digitalWrite(PIN_WATER, RELAY_ON);
}

// ======================================================
// START SPIN
// ======================================================

void startSpin() {

  if (digitalRead(PIN_LID) == HIGH) {

    errorScreen("E01", "LID OPEN");

    return;
  }

  stateStart = millis();

  stateDuration =
    (unsigned long)cfg.spinTime *
    60000UL;

  state = ST_SPIN;

  stopEverything();

  digitalWrite(PIN_SPIN, RELAY_ON);
}

// ======================================================
// UPDATE MACHINE
// ======================================================

void updateMachine() {

  if (state == ST_DELAY) {

    updateDelay();

    return;
  }

  if (state == ST_FILL) {

    updateFill();

    return;
  }

  if (state == ST_WASH) {

    updateWash();

    return;
  }

  if (state == ST_DRAIN_1) {

    updateDrain();

    return;
  }

  if (state == ST_RINSE_FILL) {

    updateRinseFill();

    return;
  }

  if (state == ST_RINSE) {

    updateRinse();

    return;
  }

  if (state == ST_DRAIN_2) {

    updateDrain();

    return;
  }

  if (state == ST_SPIN) {

    updateSpin();

    return;
  }

  if (state == ST_PAUSE) {

    updatePause();

    return;
  }
}

// ======================================================
// DELAY
// ======================================================

void updateDelay() {

  unsigned long elapsed =
    millis() - delayStart;

  if (elapsed >= delayDuration) {

    if (programType == PROGRAM_SPIN)
      startSpin();
    else
      startWash();

    return;
  }

  unsigned long remaining =
    delayDuration - elapsed;

  char timeText[12];

  formatTime(remaining, timeText);

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(28, 0);
  oled.println("DELAY START");

  oled.setTextSize(2);

  oled.setCursor(28, 22);
  oled.println(timeText);

  oled.setTextSize(1);

  oled.setCursor(10, 50);
  oled.println("BACK = CANCEL");

  oled.display();

  if (pressed(btnBack)) {

    stopEverything();

    state = ST_IDLE;

    beepError();
  }
}

// ======================================================
// FILL
// ======================================================

void updateFill() {

  unsigned long elapsed =
    millis() - stateStart;

  if (elapsed >= stateDuration) {

    digitalWrite(PIN_WATER, RELAY_OFF);

    stateStart = millis();

    stateDuration =
      (unsigned long)cfg.washTime *
      60000UL;

    state = ST_WASH;

    motorDirection =
      MOTOR_FORWARD;

    motorPhaseStart =
      millis();

    motorPhaseTime = 4000;

    digitalWrite(PIN_WASH, RELAY_ON);

    beepOK();

    return;
  }

  unsigned long remaining =
    stateDuration - elapsed;

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(25, 0);
  oled.println("FILLING WATER");

  oled.setTextSize(2);

  oled.setCursor(40, 18);

  oled.print(cfg.waterLevel);
  oled.print("%");

  oled.setTextSize(1);

  oled.setCursor(5, 45);
  oled.print("Remaining: ");

  oled.print(remaining / 1000);

  oled.print("s");

  oled.setCursor(5, 57);
  oled.println("ENTER = PAUSE");

  oled.display();

  if (pressed(btnEnter)) {

    pauseMachine();
  }
}

// ======================================================
// WASH
// ======================================================

void updateWash() {

  unsigned long elapsed =
    millis() - stateStart;

  if (elapsed >= stateDuration) {

    stopEverything();

    stateStart = millis();

    stateDuration =
      3UL * 60000UL;

    state = ST_DRAIN_1;

    digitalWrite(PIN_DRAIN, RELAY_ON);

    beepOK();

    return;
  }

  motorControl();

  unsigned long remaining =
    stateDuration - elapsed;

  char timeText[12];

  formatTime(remaining, timeText);

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(38, 0);
  oled.println("WASHING");

  oled.setCursor(0, 12);

  if (cfg.washMode == MODE_HARD)
    oled.println("MODE: HARD");

  else
    oled.println("MODE: SOFT");

  oled.setCursor(75, 12);

  oled.print(cfg.waterLevel);
  oled.println("%");

  oled.setTextSize(2);

  oled.setCursor(30, 28);
  oled.println(timeText);

  oled.setTextSize(1);

  oled.setCursor(0, 53);
  oled.println("ENTER=PAUSE BACK=CANCEL");

  oled.display();

  if (pressed(btnEnter)) {

    pauseMachine();

    return;
  }

  if (pressed(btnBack)) {

    stopEverything();

    state = ST_IDLE;

    beepError();

    mainMenuScreen();
  }
}

// ======================================================
// MOTOR CONTROL
// ======================================================

void motorControl() {

  unsigned long now = millis();

  unsigned long phaseElapsed =
    now - motorPhaseStart;

  unsigned long forwardTime;
  unsigned long reverseTime;

  if (cfg.washMode == MODE_HARD) {

    // Hard wash:
    // 4 sec forward
    // 4 sec reverse

    forwardTime = 4000;
    reverseTime = 4000;

  } else {

    // Soft wash:
    // 2 sec forward
    // 5 sec pause
    // 2 sec reverse
    //
    // simplified alternating pattern

    forwardTime = 2500;
    reverseTime = 5000;
  }

  if (phaseElapsed >= motorPhaseTime) {

    motorDirection =
      !motorDirection;

    motorPhaseStart = now;

    if (motorDirection == MOTOR_FORWARD) {

      motorPhaseTime = forwardTime;

      digitalWrite(PIN_WASH, RELAY_ON);

    } else {

      motorPhaseTime = reverseTime;

      digitalWrite(PIN_WASH, RELAY_ON);
    }
  }
}

// ======================================================
// DRAIN
// ======================================================

void updateDrain() {

  unsigned long elapsed =
    millis() - stateStart;

  if (elapsed >= stateDuration) {

    stopEverything();

    // First drain → rinse fill
    if (state == ST_DRAIN_1) {

      startRinse();

      return;
    }

    // Second drain → spin
    if (state == ST_DRAIN_2) {

      startSpin();

      return;
    }
  }

  unsigned long remaining =
    stateDuration - elapsed;

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(35, 0);
  oled.println("DRAINING");

  oled.setTextSize(2);

  oled.setCursor(35, 20);

  oled.print(remaining / 1000);

  oled.print("s");

  oled.setTextSize(1);

  oled.setCursor(0, 52);
  oled.println("ENTER=PAUSE");

  oled.display();

  if (pressed(btnEnter)) {

    pauseMachine();
  }
}

// ======================================================
// RINSE START
// ======================================================

void startRinse() {

  if (cfg.rinseCount < 1)
    cfg.rinseCount = 1;

  state = ST_RINSE_FILL;

  stateStart = millis();

  stateDuration =
    map(
      cfg.waterLevel,
      5,
      80,
      15000,
      180000
    );

  digitalWrite(PIN_WATER, RELAY_ON);
}

// ======================================================
// RINSE FILL
// ======================================================

void updateRinseFill() {

  unsigned long elapsed =
    millis() - stateStart;

  if (elapsed >= stateDuration) {

    digitalWrite(PIN_WATER, RELAY_OFF);

    state = ST_RINSE;

    stateStart = millis();

    stateDuration =
      2UL * 60000UL;

    motorDirection =
      MOTOR_FORWARD;

    motorPhaseStart =
      millis();

    motorPhaseTime = 3000;

    digitalWrite(PIN_WASH, RELAY_ON);

    return;
  }

  unsigned long remaining =
    stateDuration - elapsed;

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(20, 0);
  oled.println("RINSE FILLING");

  oled.setTextSize(2);

  oled.setCursor(35, 20);

  oled.print(cfg.waterLevel);
  oled.print("%");

  oled.setTextSize(1);

  oled.setCursor(0, 50);

  oled.print("Time: ");
  oled.print(remaining / 1000);
  oled.print("s");

  oled.display();

  if (pressed(btnEnter)) {

    pauseMachine();
  }
}

// ======================================================
// RINSE
// ======================================================

void updateRinse() {

  unsigned long elapsed =
    millis() - stateStart;

  if (elapsed >= stateDuration) {

    stopEverything();

    stateStart = millis();

    stateDuration =
      3UL * 60000UL;

    state = ST_DRAIN_2;

    digitalWrite(PIN_DRAIN, RELAY_ON);

    return;
  }

  motorControl();

  unsigned long remaining =
    stateDuration - elapsed;

  char timeText[12];

  formatTime(remaining, timeText);

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(38, 0);
  oled.println("RINSING");

  oled.setCursor(0, 15);

  oled.print("Rinses: ");
  oled.print(cfg.rinseCount);

  oled.setTextSize(2);

  oled.setCursor(30, 30);
  oled.println(timeText);

  oled.setTextSize(1);

  oled.setCursor(0, 55);
  oled.println("ENTER=PAUSE");

  oled.display();

  if (pressed(btnEnter)) {

    pauseMachine();
  }
}

// ======================================================
// SPIN
// ======================================================

void updateSpin() {

  unsigned long elapsed =
    millis() - stateStart;

  if (elapsed >= stateDuration) {

    stopEverything();

    completeMachine();

    return;
  }

  unsigned long remaining =
    stateDuration - elapsed;

  char timeText[12];

  formatTime(remaining, timeText);

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(40, 0);
  oled.println("SPINNING");

  oled.setTextSize(2);

  oled.setCursor(30, 22);
  oled.println(timeText);

  oled.setTextSize(1);

  oled.setCursor(0, 50);
  oled.println("ENTER=PAUSE");

  oled.display();

  if (pressed(btnEnter)) {

    pauseMachine();
  }
}

// ======================================================
// PAUSE
// ======================================================

void pauseMachine() {

  pauseRemaining =
    stateDuration -
    (millis() - stateStart);

  stopEverything();

  state = ST_PAUSE;

  beep();
}

// ======================================================
// PAUSE SCREEN
// ======================================================

void updatePause() {

  oled.clearDisplay();

  oled.setTextSize(2);

  oled.setCursor(25, 5);
  oled.println("PAUSE");

  oled.setTextSize(1);

  oled.setCursor(10, 35);
  oled.println("ENTER = RESUME");

  oled.setCursor(10, 50);
  oled.println("BACK = CANCEL");

  oled.display();

  if (pressed(btnEnter)) {

    resumeMachine();

    return;
  }

  if (pressed(btnBack)) {

    stopEverything();

    state = ST_IDLE;

    beepError();

    mainMenuScreen();
  }
}

// ======================================================
// RESUME
// ======================================================

void resumeMachine() {

  stateStart =
    millis();

  stateDuration =
    pauseRemaining;

  // Restart output according to
  // previous process.

  if (programType == PROGRAM_SPIN) {

    state = ST_SPIN;

    digitalWrite(PIN_SPIN, RELAY_ON);

  } else {

    // Resume wash/rinse.
    // If the machine was in pause,
    // motor resumes safely.

    state = ST_WASH;

    digitalWrite(PIN_WASH, RELAY_ON);

    motorDirection =
      MOTOR_FORWARD;

    motorPhaseStart =
      millis();

    motorPhaseTime = 3000;
  }

  beepOK();
}

// ======================================================
// COMPLETE
// ======================================================

void completeMachine() {

  stopEverything();

  state = ST_COMPLETE;

  completeBeep();

  oled.clearDisplay();

  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(1);

  oled.setCursor(28, 0);
  oled.println("DIGITECH DYNAMO");

  oled.setTextSize(2);

  oled.setCursor(5, 20);
  oled.println("COMPLETE!");

  oled.setTextSize(1);

  oled.setCursor(15, 48);
  oled.println("REMOVE CLOTHES");

  oled.display();
}

// ======================================================
// LID SAFETY
// ======================================================

void lidSafety() {

  bool open =
    digitalRead(PIN_LID) == HIGH;

  if (open) {

    if (state == ST_FILL ||
        state == ST_WASH ||
        state == ST_DRAIN_1 ||
        state == ST_RINSE_FILL ||
        state == ST_RINSE ||
        state == ST_DRAIN_2 ||
        state == ST_SPIN) {

      stopEverything();

      state = ST_ERROR;

      errorScreen("E01", "LID OPEN");
    }
  }
}

// ======================================================
// ERROR
// ======================================================

void errorScreen(const char *code,
                 const char *text) {

  stopEverything();

  state = ST_ERROR;

  beepError();

  while (true) {

    oled.clearDisplay();

    oled.setTextColor(SSD1306_WHITE);

    oled.setTextSize(2);

    oled.setCursor(35, 0);
    oled.println(code);

    oled.setTextSize(1);

    oled.setCursor(10, 25);
    oled.println(text);

    oled.setCursor(10, 38);
    oled.println("MACHINE STOPPED");

    oled.setCursor(5, 52);

    if (digitalRead(PIN_LID) == HIGH)
      oled.println("CLOSE LID");
    else
      oled.println("PRESS ENTER");

    oled.display();

    if (digitalRead(PIN_LID) == LOW) {

      if (pressed(btnEnter)) {

        beepOK();

        state = ST_IDLE;

        mainMenuScreen();

        return;
      }
    }

    if (pressed(btnBack)) {

      state = ST_IDLE;

      mainMenuScreen();

      return;
    }
  }
}

// ======================================================
// STOP EVERYTHING
// ======================================================

void stopEverything() {

  digitalWrite(PIN_WATER, RELAY_OFF);
  digitalWrite(PIN_WASH, RELAY_OFF);
  digitalWrite(PIN_DRAIN, RELAY_OFF);
  digitalWrite(PIN_SPIN, RELAY_OFF);
}

// ======================================================
// EEPROM SAVE
// ======================================================

void saveSettings() {

  cfg.magic = EEPROM_MAGIC;

  EEPROM.put(0, cfg);
}

// ======================================================
// EEPROM LOAD
// ======================================================

void loadSettings() {

  EEPROM.get(0, cfg);

  if (cfg.magic != EEPROM_MAGIC) {

    resetSettings();
  }

  if (cfg.washTime < 1 ||
      cfg.washTime > 120)
    cfg.washTime = 10;

  if (cfg.spinTime < 1 ||
      cfg.spinTime > 60)
    cfg.spinTime = 5;

  if (cfg.waterLevel < 5 ||
      cfg.waterLevel > 80)
    cfg.waterLevel = 40;

  if (cfg.washMode > MODE_DRAIN)
    cfg.washMode = MODE_SOFT;

  if (cfg.rinseCount < 1 ||
      cfg.rinseCount > 3)
    cfg.rinseCount = 1;
}

// ======================================================
// RESET EEPROM
// ======================================================

void resetSettings() {

  cfg.magic = EEPROM_MAGIC;

  cfg.washTime = 10;

  cfg.spinTime = 5;

  cfg.waterLevel = 40;

  cfg.washMode = MODE_SOFT;

  cfg.delayHours = 0;

  cfg.delayMinutes = 0;

  cfg.childLock = 0;

  cfg.rinseCount = 1;

  saveSettings();
}

// ======================================================
// BUTTON PRESS
// ======================================================

bool pressed(Button &b) {

  bool reading =
    digitalRead(b.pin);

  if (reading == LOW &&
      b.last == HIGH) {

    delay(25);

    b.last = LOW;

    b.downTime = millis();

    return true;
  }

  if (reading == HIGH) {

    b.last = HIGH;
  }

  return false;
}

// ======================================================
// LONG PRESS
// ======================================================

bool longPressed(Button &b) {

  bool reading =
    digitalRead(b.pin);

  if (reading == LOW) {

    if (b.downTime == 0)
      b.downTime = millis();

    if (millis() - b.downTime >= 3000) {

      while (digitalRead(b.pin) == LOW)
        delay(10);

      b.downTime = 0;

      return true;
    }
  }

  else {

    b.downTime = 0;
  }

  return false;
}

// ======================================================
// TIME FORMAT
// ======================================================

void formatTime(unsigned long ms,
                char *buffer) {

  unsigned long totalSeconds =
    ms / 1000UL;

  unsigned int minutes =
    totalSeconds / 60UL;

  unsigned int seconds =
    totalSeconds % 60UL;

  sprintf(
    buffer,
    "%02u:%02u",
    minutes,
    seconds
  );
}

// ======================================================
// BUZZERS
// ======================================================

void beep() {

  tone(PIN_BUZZER, 2200, 70);
}

// ======================================================

void beepOK() {

  tone(PIN_BUZZER, 2200, 80);

  delay(120);

  tone(PIN_BUZZER, 2800, 100);
}

// ======================================================

void beepError() {

  for (byte i = 0; i < 3; i++) {

    tone(PIN_BUZZER, 900, 180);

    delay(230);
  }
}

// ======================================================

void completeBeep() {

  for (byte i = 0; i < 5; i++) {

    tone(PIN_BUZZER, 2600, 120);

    delay(170);
  }
}