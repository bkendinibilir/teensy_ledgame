#include <Bounce.h>
#include <math.h>

#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define BLINKLED_UNIT 250
#define MORSECODE_UNIT 600
#define INPUT_TIMEOUT 750
#define APP_TIMEOUT 900000 
// 900000 = 15 * 60 * 1000ms = 15mins

#define PIN_BTN PIN_C7
#define PIN_LED PIN_D1
#define PIN_LED_L1 PIN_F7
#define PIN_LED_L2 PIN_F6
#define PIN_LED_L3 PIN_F5
#define PIN_LED_L4 PIN_F4

Bounce button = Bounce(PIN_BTN, 25);
elapsedMillis sinceButton;
elapsedMillis sinceAnyAction;

int state;

/*************************************************************************************/

// return int(a ^ b)
int intpow(int a, int b) {
  int result = 1;
  
  for(int i = 0; i < b; i++) {
    result *= a;
  }
  return result;
}

// blink led c times: turn on led, wait high ms, turn off led, wait low ms
void blink_led(int count, int high_ms, int low_ms) {
  for(int i = 0; i < count; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(high_ms);
    digitalWrite(PIN_LED, LOW);
    delay(low_ms);
  }
}

// blink decimal, most significant digit first
void blink_dec(int a, int unit_ms) {
  for(int i = 6; i >= 0; i--) {
    int d = intpow(10, i);
    int q = a / d;
    if(q > 0) {
      blink_led(q, unit_ms, unit_ms);
      delay(3*unit_ms);
      a = a % d;
    }
  }
}

// blink 3 decimals
void blink_3dec(int a, int b, int c, int unit_ms) {
  blink_dec(a, unit_ms);
  delay(7*unit_ms);
  blink_dec(b, unit_ms);
  delay(7*unit_ms);
  blink_dec(c, unit_ms);
}

int read_button(void) {
  button.update();
  if(button.risingEdge()) {
    sinceButton = 0;
    sinceAnyAction = 0;
    return 1;
  }
  return 0;
}
  
// read single digit, finished if return > 0
int read_digit(int wait_ms) {
  static int count = 0;
  button.update();
 
  // risingEdge
  if(button.risingEdge()) {
    Serial.println("button");
    count++;
    sinceButton = 0;
    sinceAnyAction = 0;
  }
  if (count > 0 && sinceButton > wait_ms) {
    blink_led(count, 150, 100);
    int c = count;
    count = 0;
    sinceButton = 0;
    return c;
  }
  return 0;
}

// read decimal from x digits, finished if return > 0
int read_dec(int digits) {
  static int dec = 0;
  static int exponent;
 
  if(dec == 0) {
    exponent = digits - 1;
  }
  
  int c = read_digit(INPUT_TIMEOUT);
  
  if(c > 0) {
    dec += intpow(10, exponent) * c;
    if(exponent == 0) {
      int result = dec;
      dec = 0;
      return result;
    } 
    exponent--;
  }
  return 0;
}

// verify 2 decimals, return: -1 = false, 1 = true
int verify_input(int no, int code) {
  delay(500);
  if (no == code) {
    blink_led(1, 2000, 1000);
    return 1;
  } else {
    blink_led(3, 50, 50);
    delay(1000);
    return -1;
  }
}

// check if read decimal is number no, return: -1 = false, 1 = true
int check_number(int no) {
  int input = read_dec(int(log10(no) + 1));
  if(input > 0) {
    return verify_input(input, no);
  }
  return 0;
}

// check if read decimal is the fourth of a series, return: -1 = false, 1 = true
int check_4series(int a, int b, int c, int d) {
  static int step = 0;

  switch(step) {
    case 0:
      blink_3dec(a, b, c, BLINKLED_UNIT);
      step++;
    case 1:
      int ret = check_number(d);
      if(ret == -1 || ret == 1) {
        step = 0;
      } 
      return ret;
  }
}

// blink string as morse code with unit length of unit_ms
void blink_morse(const char *code, int unit_ms) {
  char *mc[] = { "SL", "LSSS", "LSLS", "LSS", "S", "SSLS", "LLS", "SSSS", "SS", "SLLL", "LSL", // A-K
    "SLSS", "LL", "LS", "LLL", "SLLS", "LLSL", "SLS", "SSS", "L", "SSL", "SSSL", "SLL", // L-W
    "LSSL", "LSLL", "LLSS", // X-Z
    "LLLLL", "SLLLL", "SSLLL", "SSSLL", "SSSSL", "SSSSS", "LSSSS", "LLSSS", "LLLSS", "LLLLS" }; // 0-9 
  int pos = 0;
  
  for(int i = 0; i < strlen(code); i++) {
    if(code[i] >= 0x41 && code[i] <= 0x5A) {
      pos = code[i] - 0x41;
    } else if(code[i] >= 0x30 && code[i] <= 0x39) {
      pos = 26 + code[i] - 0x30;
    } else if(code[i] == 0x20) {
      delay(7*unit_ms); 
      continue;
    } else {
      continue;
    }
    
    for(int j=0; j < strlen(mc[pos]); j++) {
      if(mc[pos][j] == 'S') {
        blink_led(1, unit_ms, unit_ms);
      } else if(mc[pos][j] == 'L') {
        blink_led(1, 3*unit_ms, unit_ms);
      }
    }
    
    delay(3*unit_ms);
  }
}

/*************************************************************************************/

void setup()
{
  // disable USB and set all i/o to output
  Serial.end();
  for (int i=0; i<46; i++) {
    pinMode(i, OUTPUT);
  }
  
  pinMode(PIN_LED, OUTPUT);       // LED
  pinMode(PIN_LED_L1, OUTPUT);    // LED: Level 1
  pinMode(PIN_LED_L2, OUTPUT);    // LED: Level 2
  pinMode(PIN_LED_L3, OUTPUT);    // LED: Level 3
  pinMode(PIN_LED_L4, OUTPUT);    // LED: Level 4
  pinMode(PIN_BTN, INPUT_PULLUP); // Pushbutton
  
  digitalWrite(PIN_LED, HIGH);
   
  state = 0;
}

void pinInterrupt(void)
{
    detachInterrupt(PIN_BTN);
}

void sleepNow()
{
    // go to sleep
    attachInterrupt(PIN_BTN, pinInterrupt, LOW);
    delay(100);    
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_mode();
    
    // wake up after interrupt
    sleep_disable();  
}

void loop()
{ 
  static int input;
  int a;
  
  if(state > 1 && sinceAnyAction > APP_TIMEOUT) {
    // reset to stage 0
    sinceAnyAction = 0;
    state = 0;
  }
  
  switch(state) {
    case 0:
      //sleepNow();
      digitalWrite(PIN_LED_L1, LOW);
      digitalWrite(PIN_LED_L2, LOW);
      digitalWrite(PIN_LED_L3, LOW);
      digitalWrite(PIN_LED_L4, LOW); 
      blink_led(3, 50, 50);
      read_button();

      state++;
    case 1:
      if(check_number(1975) == 1) {
        digitalWrite(PIN_LED_L1, HIGH);
        state++;
      }
      break;
    case 2:
      if(check_4series(13, 21, 34, 55) == 1) {
        digitalWrite(PIN_LED_L2, HIGH);
        state++;
      }  
      break;
    case 3:
      if(check_4series(7, 11, 13, 17) == 1) {
        digitalWrite(PIN_LED_L3, HIGH);
        state++;
      }  
      break;
    case 4:
      if(check_4series(1, 2, 6, 24) == 1) {
        digitalWrite(PIN_LED_L4, HIGH);
        state++;
      }  
      break;
    case 5:
      blink_morse("N 32 05 1E6   O 008 0A 1A3", MORSECODE_UNIT);
      state++;
    case 6:
      if(read_button() == 1) {
        state--;
      }
  }
  delay(10);
}


