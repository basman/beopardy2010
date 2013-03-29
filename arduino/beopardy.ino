/*
  CCC beopardy serial protocol
  
  See http://www.42.org/~sec/B/beopardy-2010/RTFM.serial
  
  Hardware connections / pin assignments:
  
  Button 1: pin 7,  D4
  Button 2: pin 8,  D5
  Button 3: pin 12, D9
  Button 4: pin 14, D11
  Button 5: pin 15, D12
  Button 6: pin 16, D13
  
  Lamp 1: pin  5, D2 (PWM capable)
  Lamp 2: pin  6, D3 (PWM capable)
  Lamp 3: pin  9, D6 (PWM capable)
  Lamp 4: pin 10, D7 (PWM capable)
  Lamp 5: pin 11, D8 (PWM capable)
  Lamp 6: pin  3, RESET (PWM capable)


  TODO
  
    - blinking lamps indicate players may press their buttons
    - lock players that already gave a wrong answer during current round

*/

/* PLAYERS: maximum number of players the hardware supports
 * setting this higher than the actual number of buttons currently connected is ok.
 * setting this higher than 5 will redefine the RESET pin 3.
 * maximum setting is 6 for the Arduino nano v3.0.
 */
#define PLAYERS 4

/* BUTTON_PUSHED: the TTL level on digital pins while a button is pushed
 * depends on the circuit connecting the buttons to the digital I/O pins (true or false)
 */
#define BUTTON_PUSHED true

// DEBOUNCE_COUNT: how many consecutive reads until a button is considered to be pressed
#define DEBOUNCE_COUNT 8

// consistency check on number of maximum players
#if PLAYERS < 2 || PLAYERS > 6
#error invalid numbers of \PLAYERS: PLAYERS
#endif

int ledPin = 13; // indicator for "button has been pushed"

int buttonPins[] = { 7, 8, 12, 14, 15, 16 };
int lampPins[]   = { 5, 6,  9, 10, 11,  3 };

boolean asyncMode = true;      // sync mode: PC polls board for buttons; async mode: board sends buttons anytime
int button = 0;                // indicates which button was pressed first, or zero if no button pressed yet
int debounceCounters[PLAYERS]; // debounce counters for each button

void setup() {
  // create serial object
  Serial.begin(9600);
  
  // set pin modes
  pinMode(ledPin, OUTPUT);
  
  for(int i=0; i<PLAYERS; i++) {
      pinMode(buttonPins[i], INPUT);
      pinMode(lampPins[i], OUTPUT);
  }
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

void resetBoard() {
    // TODO reset lamps
    digitalWrite(ledPin, LOW);
    button = 0;
}

// single scan sweep over all buttons with debouncing
// this function shall be called from a loop, so consecutive reads on button pins make it through debouncing
int scanButtons() {
    for(int i=0; i<PLAYERS; i++) {
        if(digitalRead(buttonPins[i]) == BUTTON_PUSHED) {
            debounceCounters[i]++;
            if(debounceCounters[i] >= DEBOUNCE_COUNT) {
                button = i+1;
// TODO activate the lamp and switch off all others
                digitalWrite(ledPin, HIGH);
                return button;
            }
        } else {
            debounceCounters[i] = 0;
        }
    }
    delay(1); // delay for 1 ms (debouncing)
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
    
  } else if (cmd == 'Q') {  // poll in sync mode
    if(asyncMode)
      Serial.println("?"); // no polling in async mode
    else
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
  } else {
    // indicate invalid or unknown command
    Serial.println("?");
  }
  Serial.flush();  
}

void loop() {
  // wait for serial input
  if(Serial.available() != 0) {
     receiveCommand(); 
  }
  
  // scan buttons only if board is cold
  if(!button) {
      if(scanButtons() && asyncMode) {
          Serial.println(button);
      }
  }
}
