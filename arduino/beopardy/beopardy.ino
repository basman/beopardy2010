/*
  CCC beopardy serial protocol
  
  See http://www.42.org/~sec/B/beopardy-2010/RTFM.serial
  
  Hardware connections / pin assignments:
  
  Button 1: pin D2
  Button 2: pin D4
  Button 3: pin D7
  Button 4: pin D8
  Button 5: pin D12
  Button 6: pin D13 (onboard LED)
  
  Lamp 1: pin  D3  (PWM capable)
  Lamp 2: pin  D5  (PWM capable)
  Lamp 3: pin  D6  (PWM capable)
  Lamp 4: pin  D9  (PWM capable)
  Lamp 5: pin  D10 (PWM capable)
  Lamp 6: pin  D11 (PWM capable)

*/

/* PLAYERS: maximum number of players the hardware supports
 * setting this higher than the actual number of buttons currently connected is ok.
 * setting this higher than 5 will redefine the onboard LED
 * maximum setting is 6 for the Arduino nano v3.0.
 */
#define PLAYERS 6

int buttonPins[] = { 2, 4, 7, 8, 12, 13 };
int lampPins[]   = { 3, 5, 6, 9, 10, 11 };

/* BUTTON_PUSHED: the TTL level on digital pins while a button is pushed
 * depends on the circuitry that connects the buttons to the digital I/O pins (true or false)
 */
#define BUTTON_PUSHED LOW

// DEBOUNCE_COUNT: how many consecutive reads until a button is considered to be pressed
#define DEBOUNCE_COUNT 8

// consistency check on number of maximum players
#if PLAYERS < 2 || PLAYERS > 6
#error invalid numbers of \PLAYERS: PLAYERS
#endif

#if PLAYERS < 6
int ledPin = 13; // indicator for "button has been pushed"
#endif

bool asyncMode = true;      // sync mode: PC polls board for buttons; async mode: board sends buttons anytime
int button = 0;                // indicates which button was pressed first, or zero if no button pressed yet, or -1 if buttons inactive by command 'N'
int debounceCounters[PLAYERS]; // debounce counters for each button
unsigned char suppressedButtons;     // bitmask that indicates suppressed buttons (player gave wrong answer)

// these arrays control the lamp flickering
typedef struct struct_flash {
    unsigned short repetitions;      // how many flashes
    unsigned short attack_decay;     // attach and decay rate in ms
    unsigned short on_duration;      // hold duration in ms
    unsigned short off_duration;     // pause before next repetition in ms
} t_flash;

const t_flash flashes_odd[] = {
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 },  // 500 ms pause
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 },  // 500 ms pause
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 },  // 500 ms pause
  {  3, 500, 500, 600 } // 4 gentle faded flashes
};
const t_flash flashes_even[] = {
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 },  // 500 ms pause
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 },  // 500 ms pause
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 },  // 500 ms pause
  {  2, 500, 500, 600 }, // 4 gentle faded flashes
  {  3, 500, 500, 600 }, // 4 gentle faded flashes
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 },  // 500 ms pause
  { 12, 5, 20, 20 }, // 20 quick flashes
  {  1, 0, 0, 300 }  // 500 ms pause
};

const t_flash *flash_controls[2] = { flashes_even, flashes_odd };

#define FLASH_N_ODD sizeof(flashes_odd)/sizeof(flashes_odd[0])
#define FLASH_N_EVEN sizeof(flashes_even)/sizeof(flashes_even[0])
#define LOOP_IDX_ODD 2
#define LOOP_IDX_EVEN 7


int lampSequenceIdx[PLAYERS];  // index to flash sequence
int lampRepetition[PLAYERS];   // sequence repetition counter
int lampProgress[PLAYERS];     // sub-sub-index to progress of flash phase


void setup() {
  // create serial object
  Serial.begin(9600);

#if PLAYERS < 6
  // set pin modes
  pinMode(ledPin, OUTPUT);
#endif

  for(int i=0; i<PLAYERS; i++) {
      pinMode(buttonPins[i], INPUT);
      // activate pull-up resistors
      digitalWrite(buttonPins[i], HIGH);
      pinMode(lampPins[i], OUTPUT);
      digitalWrite(lampPins[i], LOW);
  }

  randomSeed(analogRead(0));
  resetBoard();
}

// indicate which buttons are currently pressed and return bitmask
int currentButtons() {
    int curButtons = 0;
    for(int i=0; i<PLAYERS; i++) {
        if(digitalRead(buttonPins[i]) == BUTTON_PUSHED)
            curButtons |= (1<<i);
    }
    return curButtons;
}

// extract first active button from bitmask
int bitmask2button(int bitmask) {
  for(int i=0; i<PLAYERS; i++) {
    if((bitmask>>i) & 1 != 0) {
        return i+1;
    }
  }
  return 0;
}

void resetLamps() {
  for(int i=0; i<PLAYERS; i++) {
    digitalWrite(lampPins[i], LOW); // switch off all lamps
    lampSequenceIdx[i] = 0;         // reset animation state
    lampRepetition[i]  = 0;
    lampProgress[i]    = 0;
  }
}

void suppressAllButtons() {
  for(int i=0; i<PLAYERS; i++) {
    suppressButton(i+1);
  }
}

void activateLamp(int index) {
  resetLamps();
  digitalWrite(lampPins[index], HIGH);
}

void resetBoard() {
    resetLamps();
    suppressedButtons = 0;
#if PLAYERS < 6
    digitalWrite(ledPin, LOW);
#endif
    button = 0;
}

void suppressButton(int button_nr) {
    suppressedButtons |= (1<<(button_nr-1));
    digitalWrite(lampPins[button_nr-1], LOW);
}

// single scan sweep over all buttons with debouncing
// this function shall be called from a loop, so consecutive reads on button pins make it through debouncing
int scanButtons() {
    static unsigned long lastScan = millis();

    // debouncing interval between button readouts
    if (millis()-lastScan < 1)
        return 0;

    for(int i=0; i<PLAYERS; i++) {

        if((suppressedButtons & (1<<i)) != 0)
            continue;

        if(digitalRead(buttonPins[i]) == BUTTON_PUSHED) {
            debounceCounters[i]++;
            if(debounceCounters[i] >= DEBOUNCE_COUNT) {
                button = i+1;
                activateLamp(i);
#if PLAYERS < 6
                digitalWrite(ledPin, HIGH);
#endif
                return button;
            }
        } else {
            debounceCounters[i] = 0;
        }
    }
    return 0;
}

void receiveCommand() {
  // read command via USB
  int cmd = Serial.read();

  if(cmd == 'A') {  // switch to async mode (board signals pressed buttons to PC anytime)
    asyncMode = true;
    Serial.println("A");

  } else if(cmd == 'S') {  // switch to sync mode (PC polls for pressed buttons)
    asyncMode = false;
    Serial.println("S");

  } else if (cmd == 'Q' && !asyncMode) {  // poll in sync mode
      Serial.println(button);

  } else if(cmd == 'R') {  // reset board (async mode)
    int curButtons = currentButtons();
    if(curButtons == 0) {
      // reset successful (all buttons currently released)
      resetBoard();
      Serial.println("A");
    } else {
      // indicate currently pressed button (only one with lowest index)
      Serial.println(bitmask2button(curButtons));
    }

  } else if(cmd == 'O' && button > 0) { // oops, button pressed accidentially - partial reset
    int curButtons = currentButtons();
    if(curButtons == 0) {
      button = 0;
      resetLamps();
      Serial.println("A");
    } else {
      // indicate currently pressed button (only one with lowest index)
      Serial.println(bitmask2button(curButtons));
    }

  } else if(cmd == 'F' && button > 0) { // answer was incorrect, ignore active button
    int curButtons = currentButtons();
    if(curButtons == 0) {
      suppressButton(button);
      button = 0;
      resetLamps();
      Serial.println("A");
    } else {
      // indicate currently pressed button (only one with lowest index)
      Serial.println(bitmask2button(curButtons));
    }

  } else if(cmd == 'N') {  // silence all lamps and buttons
    resetLamps();
    // DISABLED because the sequence R,N,1,F would leave all buttons suppressed
    //suppressAllButtons();
    button = -1; // disable buttons without messing with suppressedButtons
    Serial.println("A");

  } else if(cmd >= '1' && cmd < '1' + PLAYERS ) {  // simulate button pushed
    if(button <= 0) {   // no button pressed yet
      button = cmd - '1' + 1;
      activateLamp(button-1);
#if PLAYERS < 6
      digitalWrite(ledPin, HIGH);
#endif
      Serial.println("A");
    } else {
      Serial.println("?");
    }
  } else if(cmd == '\n' || cmd == '\r') {
    // ignore newline characters
  } else {
    // indicate invalid or unknown command
    //Serial.print(cmd);
    Serial.println("?");
  }
  Serial.flush();  // this waits until all data has been transmitted (it does not discard incoming data)
}

// advance lamp animation by one step (= 1 ms)
void animateLamps() {
  static unsigned long lastStep = 0;

  // animation time resolution is 1 ms
  if(millis()-lastStep < 1)
    return;

  lastStep = millis();

  for(int i=0; i<PLAYERS; i++) {
     if((suppressedButtons & (1<<i)) != 0)
         continue;

     // advance lampProgress through the phases (attack, hold, decay, pause)
     lampProgress[i]++;
     const t_flash *f = &flash_controls[i%2][lampSequenceIdx[i]];

     unsigned char brightness = 0;

     // find flash phase
     if(lampProgress[i] <= f->attack_decay) {
        // attack phase (fade in)
        float ratio = 255.0 * lampProgress[i] / f->attack_decay;
        brightness = (unsigned char)ratio;

     } else if(lampProgress[i] <= f->attack_decay
                                + f->on_duration) {
        // on phase
        brightness = 255;

     } else if(lampProgress[i] <= f->attack_decay
                                + f->on_duration
                                + f->attack_decay) {
        // decay phase (fade out)
        float ratio = 255.0 * (f->attack_decay - (lampProgress[i] - f->attack_decay - f->on_duration)) / f->attack_decay;
        brightness = (unsigned char)ratio;

     } else if(lampProgress[i] <= f->attack_decay
                                + f->on_duration
                                + f->attack_decay
                                + f->off_duration) {
        // off phase (rest dark until next repetition)
        brightness = 0;

     } else {
        // next repetition
        lampProgress[i] = 0;
        lampRepetition[i]++;
        if(lampRepetition[i] >= f->repetitions) {
            lampRepetition[i] = 0;
            lampSequenceIdx[i]++;
            if(i%2 == 0 && lampSequenceIdx[i] >= FLASH_N_EVEN) {
                lampSequenceIdx[i] = LOOP_IDX_EVEN;
            } else if(i%2 != 0 && lampSequenceIdx[i] >= FLASH_N_ODD) {
                lampSequenceIdx[i] = LOOP_IDX_ODD;
            }
        }
     }

     // send current brightness to lamp
     analogWrite(lampPins[i], brightness);
  }
}

void loop() {
  // watch out for serial input
  if(Serial.available() > 0) {
     receiveCommand();
  }

  // scan buttons only if no button pressed
  if(button == 0) {
      // blink buttons that are still in the game
      animateLamps();

      if(scanButtons() && asyncMode) {
          Serial.println(button);
      }
  }
}
