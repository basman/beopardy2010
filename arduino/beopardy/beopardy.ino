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
int button = 0;                // indicates which button was pressed first, or zero if no button pressed yet
int debounceCounters[PLAYERS]; // debounce counters for each button
unsigned char suppressedButtons;     // bitmask that indicates suppressed buttons (player gave wrong answer)

// these arrays control the lamp flickering
// TODO control transition speed
const unsigned short lampHold[]  = {  50, 100,  50, 100,  50, 100,  50, 100, 400, 2500 }; // unit: ms
const unsigned char lampValues[] = { 255,  10, 255,  10, 255,  10, 255,  10, 255,    0 }; // PWM duty cycle between 0 and 255
#define ANIM_STEPS sizeof(lampHold)/sizeof(lampHold[0])
#define ANOM_LOOP_IDX 8 // repeat animation at this element index

int lampProgressIdx[PLAYERS];
int lampPauses[PLAYERS];


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
    digitalWrite(lampPins[i], LOW);  // switch off all lamps
    lampProgressIdx[i] = 0;          // reset animation state
    lampPauses[i] = 0;               // initialize animation pause
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

  } else if(cmd == 'O' && button != 0) { // oops, button pressed accidentially - partial reset
    int curButtons = currentButtons();
    if(curButtons == 0) {
      button = 0;
      resetLamps();
      Serial.println("A");
    } else {
      // indicate currently pressed button (only one with lowest index)
      Serial.println(bitmask2button(curButtons));
    }

  } else if(cmd == 'F' && button != 0) { // answer was incorrect, ignore active button
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
    suppressAllButtons();
    Serial.println("A");

  } else if(cmd >= '1' && cmd < '1' + PLAYERS ) {  // simulate button pushed
    if(button == 0) {   // no button pressed yet
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

// two quick fade-in/fade-out flashes
void animateLamps() {
  static unsigned long lastStep = 0;

  // animation time resolution is 1 ms
  if(millis()-lastStep < 1)
    return;

  lastStep = millis();

  for(int i=0; i<PLAYERS; i++) {
     if((suppressedButtons & (1<<i)) != 0)
         continue;

     if(lampPauses[i] > 0) {
         lampPauses[i]--;

     } else {
        // advance to next animation element
        lampProgressIdx[i] += 1;
        if(lampProgressIdx[i] >= ANIM_STEPS) {
          lampProgressIdx[i] = ANOM_LOOP_IDX - 1;
          lampPauses[i] = random(10,400); // make lamps divert (after one animation completed)
        } else {
            analogWrite(lampPins[i], lampValues[lampProgressIdx[i]]);
            lampPauses[i] = lampHold[lampProgressIdx[i]];
        }
    }
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
