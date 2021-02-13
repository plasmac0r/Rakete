/*
 * Arduino Code for Rocket 3D Model and self made LED Strips
 * Written by plasmac0r
 * Copyright 2018 Norman Doll <plasma@c0r.de>
 * 
 * Version 1.0
 * License: Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0) 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 */

#include <FastLED.h>
#include <EEPROM.h>
#include "debuglog.h"
#include "InputDebounce.h"
#include "ledani.h"

#define LED_PIN               4             // Arduino Pin for WS2812B LED Strip
#define NUM_LEDS              24            // Total Number of LEDs
#define NUM_LEDS_SIDE         12            // Special double sided setup. 12 LED each side.
#define BRIGHTNESS            250           // Master Brightness
#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB
#define UPDATES_PER_SECOND    100
#define NUM_ANI               4             // Number of animation programs
#define MAX_BRIGHTNESS        255           // Maximum brightness
#define MAX_ANISPEED          255           // Maximum animation speed
#define ANI_ADDR              1             // EEPROM address for animation program
#define BRIGHTNESS_ADDR       2             // EEPROM address for brightness setting
#define ANISPEED_ADDR         3             // EEPROM address for animation speed setting

#define BUTTON_DEBOUNCE_DELAY 20            // [ms]
#define BTN1_PIN  2                         // Arduino Pin for Button 1
#define BTN2_PIN  3                         // Arduino Pin for Button 1
static InputDebounce button1;               // not enabled yet, setup has to be called first, see setup() below
static InputDebounce button2;
bool btn1pressed = false;
bool btn1released = false;
int btn1hold = 0;
int btn1holdreleased = 0;
bool btn2pressed = false;
bool btn2released = false;
int btn2hold = 0;
int btn2holdreleased = 0;

// Set seperate front and back segments of LED-Tower
CRGBArray<NUM_LEDS> leds;
CRGBSet ledsRear( leds(0,11) ); // 0=bottom led, 11=top led
CRGBSet ledsFront( leds(12,23) ); // 12=top led, 23=bottom led
#define LEDF_B 0  // Index of front bottom LED
#define LEDF_T 11 // Index of front top LED
#define LEDR_B 23 // Index of rear bottom LED
#define LEDR_T 12 // Index of rear top LED


// Inspired by FastLED examples:
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100 
#define COOLING  20

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 200

// Palette 0=bottom part, 255=top part
// pos, R, G, B
/*
DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
  0, 255,  255, 255,   // white
 80,  96,   96,  96,   // grey
170,  255, 160,   0,   // orange
220,  255, 255,   0,   // light yellow
240,  255, 255,   255,   // white
255,  0, 120, 255 }; // light blue
*/
DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
  0, 255,  255, 255,   // white
100,  255, 180,   0,   // orange
220,  255, 255,   0,   // light yellow
255,  255, 255, 205    // light blue
};
CRGBPalette16 rocketPal = heatmap_gp;

DEFINE_GRADIENT_PALETTE( jetwash_gp ) {
  0,   0,   0, 255,     // blue
135,  16,  32, 255,     // light blue
255, 255, 250,   0      // yellow
};
CRGBPalette16 jetwashPal = jetwash_gp;

DEFINE_GRADIENT_PALETTE( firetail_gp ) {
  0, 255, 250,   0,     // yellow
180, 255, 150,   0,     // orange-yellow
255, 120, 255, 250      // white
};
CRGBPalette16 firetailPal = firetail_gp;

const int numleds = 12;

CRGBPalette16 palette;
TBlendType    blending;

uint8_t state = 0;

static uint8_t curAni = 0;            // Aktuell abspielende Animation
static uint8_t savedAni = 0;          // Im EEPROM gespeicherte Animation
static uint8_t curBrightness = 0;     // Aktuell abspielende Animation
static uint8_t savedBrightness = 0;   // Im EEPROM gespeicherte Brightness
static uint8_t curSpeed = 0;          // Aktuell abspielende Animation
static uint8_t savedSpeed = 0;        // Im EEPROM gespeicherter Speed

// Timing
unsigned long curMillis;
unsigned long startMillis;


/*  ***************************************************************************************************************
 *  Prototypes
 *  ***************************************************************************************************************
 */
void playAni( uint8_t ani, uint8_t aniSpeed, uint8_t aniBrightness );
uint8_t chgAni( uint8_t ani, int8_t change );
uint8_t chgBrightness( uint8_t brightness, int8_t change );
uint8_t chgAniSpeed( uint8_t anispeed, int8_t change );
void brightnessMode();
void turnOff( uint8_t ani, uint8_t brightness, uint8_t anispeed );
void aniSpeedMode();
void saveAni( uint8_t ani );
uint8_t loadAni();
void saveBrightness( uint8_t brightness );
uint8_t loadBrightness();
void saveAniSpeed( uint8_t anispeed );
uint8_t loadAniSpeed();
void ledAniUpDown(CRGB color);
void ledAniRocketStart();
void ledAniThruster(int jetwash, int firetail);
void ledAniRainbow(CRGBPalette16 &palette, TBlendType &blending);
void button_pressedCallback(uint8_t pinIn);
void button_releasedCallback(uint8_t pinIn);
void button_pressedDurationCallback(uint8_t pinIn, unsigned long duration);
void button_releasedDurationCallback(uint8_t pinIn, unsigned long duration);

/*  ***************************************************************************************************************
 *  Initialization
 *  ***************************************************************************************************************
 */
void setup() {
    // init serial
    Serial.begin(115200);
    LINFO("Rakete LED - START");
    
    delay( 100 ); // power-up safety delay

    // Button INIT 
    // ------------------------------------------------------------------------------------------------------------
    // register callback functions (shared, used by all buttons)
    button1.registerCallbacks(button_pressedCallback, button_releasedCallback, button_pressedDurationCallback, button_releasedDurationCallback);
    button2.registerCallbacks(button_pressedCallback, button_releasedCallback, button_pressedDurationCallback, button_releasedDurationCallback);
    // setup input buttons (debounced)
    button1.setup(BTN1_PIN, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);
    button2.setup(BTN2_PIN, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);

    // LED INIT
    // ------------------------------------------------------------------------------------------------------------
    //FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    leds = CRGB::Black;  // Turn off all LEDs
    palette = RainbowColors_p;
    blending = LINEARBLEND;

    // Load settings from eeprom
    curAni = loadAni();
    curBrightness = loadBrightness();
    curSpeed = loadAniSpeed();
    
    // Set brightness
    FastLED.setBrightness( curBrightness );

    // Start timer
    curMillis =  millis();
    startMillis = curMillis;

}

/*  ***************************************************************************************************************
 *  Main Loop
 *  ***************************************************************************************************************
 */
void loop() {

  curMillis = millis();

  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( random() );
  
  // poll button state
  button1.process(curMillis); // callbacks called in context of this function
  button2.process(curMillis);

/*
  LDEBUG10("state:", state);
  LDEBUG10(" btn1pressed:", btn1pressed);
  LDEBUG10(" btn1hold:", btn1hold);
  LDEBUG10(" btn1released:", btn1released);
  LDEBUG10(" btn2pressed:", btn2pressed);
  LDEBUG10(" btn2hold:", btn2hold);
  LDEBUG10(" btn2released:", btn2released);
  LDEBUG("");
*/

  // Button State Machine
  switch( state ) {
    case 0: { 
        // Off state - irgendeine Taste drücken zum einschalten. Verwendet zuletzt beim Ausschalten gespeicherte Einstellungen
        
        // Play last saved ani on any key press
        if( (btn1pressed == 1 && btn1released) || (btn2pressed == 1 && btn2released) ) {
          // Load settings from eeprom
          curAni = loadAni();
          curBrightness = loadBrightness();
          curSpeed = loadAniSpeed();
          
          
          btn1pressed = 0; 
          btn2pressed = 0; 
          btn1released = 0; 
          btn2released = 0; 
          btn1hold = 0; 
          btn2hold = 0; 
          state = 1; 
        }            
        break; 
      }
    case 1: { 
        // Play/On state - Spielt animation
        
        // Play previous animation
        if( btn1hold < 600 && btn1pressed && btn1released && btn2pressed == 0) { 
          curAni = chgAni(curAni, -1); 
          
          btn1pressed = 0; 
          btn1released = 0; 
          btn1hold = 0; 
        }
        // Play next animation
        else if( btn2hold < 600 && btn2pressed && btn2released && btn1pressed == 0) { 
          curAni = chgAni(curAni, +1); 
          
          btn2pressed = 0; 
          btn2released = 0; 
          btn2hold = 0; 
        }
        // Enter brightness setting mode
        else if( btn1hold >= 600 && btn2hold == 0 && btn1released) { 
          brightnessMode();
           
          btn1hold = 0; 
          btn1pressed = 0; 
          btn1released = 0;
          state = 2; 
        }
        // Enter speed setting mode
        else if( btn2hold >= 600 && btn1hold == 0 && btn2released) { 
          aniSpeedMode(); 
          
          btn2hold = 0; 
          btn2pressed = 0; 
          btn2released = 0;
          state = 3; 
        }
        // Save settings and turn off all LEDs
        else if( btn1hold >= 2000 && btn2hold >= 2000 && btn1released && btn2released) {

          // Turn off LEDs and save settings to eeprom
          turnOff(curAni, curBrightness, curSpeed); 
          
          btn1hold = 0; 
          btn2hold = 0; 
          btn1pressed = 0; 
          btn2pressed = 0; 
          btn1released = 0;
          btn2released = 0;
          state = 0; 
        }
        // Should not happen: Keypress of both buttons to short. Reset keypress.
        else if( btn1hold < 2000 && btn2hold < 2000 && btn1released && btn2released ) {
          btn1hold = 0; 
          btn2hold = 0; 
          btn1pressed = 0; 
          btn2pressed = 0;
          btn1released = 0;
          btn2released = 0;
          state = 1;
        }



        break; 
      }
    
    case 2: { 
        // BrightnessMode

        // Decrease brightness
        if( btn1hold < 600 && btn1pressed && btn1released && btn2pressed == 0 ) { 
          curBrightness = chgBrightness(curBrightness, -1);
          
          btn1pressed = 0; 
          btn1released = 0; 
          btn1hold = 0; 
        }         
        // Increase brightness
        else if( btn2hold < 600 && btn2pressed && btn2released && btn1pressed == 0 ) { 
          curBrightness = chgBrightness(curBrightness, +1);
          
          btn2pressed = 0; 
          btn2released = 0; 
          btn2hold = 0; 
        }        
        // Button is held, decrease brightness faster
        else if( btn1hold >= 600 && btn2hold == 0)    { 
          curBrightness = chgBrightness(curBrightness, -5);
          delay(50);

          if( btn1released ) {
            btn1hold = 0; 
            btn1pressed = 0; 
            btn1released = 0;
          }
        }
        // Button is held, increase brightness faster
        else if( btn2hold >= 600 && btn1hold == 0 ) { 
          curBrightness = chgBrightness(curBrightness, +5);
          delay(50);
          
          if( btn2released ) {
            btn2hold = 0; 
            btn2pressed = 0;
            btn2released = 0;
          }
        }
        // Save brightness and exit setting mode
        else if( btn1hold >= 600 && btn2hold >= 600  && btn1released  && btn2released) { 
          saveBrightness(curBrightness); 
          
          btn1hold = 0; 
          btn2hold = 0; 
          btn1pressed = 0; 
          btn2pressed = 0;
          btn1released = 0;
          btn2released = 0;
          state = 1; 
        } 
        // Should not happen: Keypress of both buttons to short. Reset keypress.
        else if( btn1released && btn2released ) {
          btn1hold = 0; 
          btn2hold = 0; 
          btn1pressed = 0; 
          btn2pressed = 0;
          btn1released = 0;
          btn2released = 0;
          state = 2;
        }
        
        break; 
      }
    case 3: { 
        // AniSpeedMode

        // Decrease speed
        if( btn1hold < 600 && btn1pressed && btn1released && btn2pressed == 0) { 
          curSpeed = chgAniSpeed(curSpeed, -1);
          
          btn1pressed = 0; 
          btn1released = 0; 
          btn1hold = 0; 
        }
        // Increase speed
        else if( btn2hold < 600 && btn2pressed && btn2released && btn1pressed == 0) { 
          curSpeed = chgAniSpeed(curSpeed, +1);
          
          btn2pressed = 0; 
          btn2released = 0; 
          btn2hold = 0; 
        }  
        // Button is held, decrease speed faster
        else if( btn1hold >= 600 && btn2hold == 0 )    { 
          curSpeed = chgAniSpeed(curSpeed, -5);
          delay(50);

          if( btn1released ) {
            btn1hold = 0; 
            btn1pressed = 0; 
            btn1released = 0;
          }
        }          
        // Button is held, increase speed faster
        else if( btn2hold >= 600 && btn1hold == 0 ) { 
          curSpeed = chgAniSpeed(curSpeed, +5);
          delay(50);
          
          if( btn2released ) {
            btn2hold = 0; 
            btn2pressed = 0;
            btn2released = 0;
          }
        }     
        // Save speed and exit setting mode
        else if( btn1hold >= 600 && btn2hold >= 600  && btn1released  && btn2released) { 
          saveAniSpeed(curSpeed); 
          
          btn1hold = 0; 
          btn2hold = 0; 
          btn1pressed = 0; 
          btn2pressed = 0; 
          btn1released = 0; 
          btn2released = 0; 
          state = 1; 
        }     
        // Should not happen: Keypress of both buttons to short. Reset keypress.
        else if( btn1released && btn2released ) {
          btn1hold = 0; 
          btn2hold = 0; 
          btn1pressed = 0; 
          btn2pressed = 0;
          btn1released = 0;
          btn2released = 0;
          state = 3;
        }        
        break; 
      }
  }

  if( state != 0 ) {
    // Speed
    // Fast = AniSpeed = 255 => Call FastLED.show() more often
    // Slow = AniSpeed = 1   => Call FastLED.show() less often
    if( curMillis - startMillis >= (255 - curSpeed) ) {
      playAni(curAni, curSpeed, curBrightness);

      // save execution time
      startMillis = curMillis;
    }
  }
}


/*  ***************************************************************************************************************
 *  Functions
 *  ***************************************************************************************************************
 */

// play animation
void playAni( uint8_t ani, uint8_t aniSpeed, uint8_t aniBrightness ) {
/*
  LDEBUG0("playAni called with params=");
  LDEBUG10(ani, ", ");
  LDEBUG10(aniSpeed, ", ");
  LDEBUG10(aniBrightness, ", ");
  LDEBUG("");
*/

  switch( ani ) {
    case 1: {
      ledAniThruster(1, 10);
      break;
    }
    case 2: {
      ledAniRocketStart();
      break;
    }
    case 3: {
      ledAniUpDown(CRGB::White);
      break;
    }
    case 4: {
      uint8_t secondHand = (millis() / 1000) % 60;
      static uint8_t lastSecond = 99;
    
      if( lastSecond != secondHand) {
          lastSecond = secondHand;
          if( secondHand ==  0)   { palette = RainbowColors_p;        blending = LINEARBLEND; }
          if( secondHand ==  10)  { palette = RainbowStripeColors_p;  blending = LINEARBLEND; }
      }
          
      ledAniRainbow(palette, blending);
    }
  }
    // Set brightness
    FastLED.setBrightness( curBrightness );
   
    // Let FastLED show the computed array leds ( leds_rear, leds_front )
    FastLED.show();
    FastLED.delay(1000 / UPDATES_PER_SECOND);

    // Set Speed [ Max Speed = 0 delay | Min Speed = MAX_ANISPEED
    //FastLED.delay(MAX_ANISPEED - curSpeed);
    
}

// Brightness mode
void brightnessMode() {
  LDEBUG("brightnessMode called");
  
}

// Animation speed mode
void aniSpeedMode() {
  LDEBUG("aniSpeedMode called");
  
}

/*
 * Turn off all LEDs and save settings to eeprom
 * 
 */
void turnOff( uint8_t ani, uint8_t brightness, uint8_t anispeed ) {
  LDEBUG0("turnOff called with params: ");
  LDEBUG10(ani, ", ");
  LDEBUG10(brightness, ", ");
  LDEBUG(anispeed);
  
  leds = CRGB::Black;  // Turn off all LEDs
  // Let FastLED show the computed array leds ( leds_rear, leds_front )
  FastLED.show();

  // Save settings to eeprom
  saveAni(ani);
  saveBrightness(brightness);
  saveAniSpeed(anispeed);
}

/*
 * change animation program, return new ani program number
 */
uint8_t chgAni( uint8_t ani, int8_t change ) {
  LDEBUG10("chgAni called with params=", ani);
  LDEBUG1(", ", change);
  int c;

  // Underflow: ani - change <= 0
  // Overflow:  ani + change >= 255
  // ani == 0 => no ani, black, off
  // range = ani_min = 1 .. ani_max = NUM_ANI

  c = ani + change;
  if( c <= 1 ) {
    // Untere Grenze = 1
    return 1;
  } 
  else if( c >= NUM_ANI ) {
    // Obere Grenze
    return NUM_ANI;
  }
  else {
    return c;
  }
  
}

/*
 * change brightness, return new brightness value
 */
uint8_t chgBrightness( uint8_t brightness, int8_t change ) {
  LDEBUG10("chgBrightness called with params=", brightness);
  LDEBUG1(", ", change);
  int c;

  // Underflow: brightness - change <= 0
  // Overflow:  brightness + change >= 255
  // brightness == 0 => black, off
  // range = brightness_min = 1 .. brightness_max = MAX_BRIGHTNESS

  c = brightness + change;
  if( c <= 1 ) {
    // Untere Grenze = 1
    return 1;
  } 
  else if( c >= MAX_BRIGHTNESS ) {
    // Obere Grenze
    return MAX_BRIGHTNESS;
  }
  else {
    return c;
  }
  
}

/*
 * change animation speed, return new animation speed value
 * 
 * TODO: Wenn 0 => stop
 */
uint8_t chgAniSpeed( uint8_t anispeed, int8_t change ) {
  LDEBUG10("chgAniSpeed called with params=", anispeed);
  LDEBUG1(", ", change);
  int c;

  // Underflow: anispeed - change <= 0
  // Overflow:  anispeed + change >= 255
  // anispeed == 0 => no ani, stop
  // range = anispeed_min = 1 .. anispeed_max = MAX_ANISPEED

  c = anispeed + change;
  if( c <= 1 ) {
    // Untere Grenze = 1
    return 1;
  } 
  else if( c >= MAX_ANISPEED ) {
    // Obere Grenze
    return MAX_ANISPEED;
  }
  else {
    return c;
  }
  
}

/*
 * save animation setting to eeprom
 * 
 */
void saveAni( uint8_t ani ) {
  LDEBUG1( "saveAni called with param=", ani );

  EEPROM.update( ANI_ADDR, ani );
  
}

/*
 * load animation from eeprom
 * 
 */
uint8_t loadAni( ) {
  LDEBUG0( "loadAni called. Val=" );
  uint8_t ani = EEPROM.read( ANI_ADDR );
  LDEBUG( ani );
  return ani;
}

/*
 * save brightness to eeprom and exit brightness setting mode
 * 
 */
void saveBrightness( uint8_t brightness ) {
  LDEBUG1( "saveBrightness called with param=", brightness );

  EEPROM.update( BRIGHTNESS_ADDR, brightness );
}

/*
 * load brightness from eeprom
 * 
 */
uint8_t loadBrightness( ) {
  LDEBUG0( "loadBrightness called. Val=" );
  uint8_t brightness = EEPROM.read( BRIGHTNESS_ADDR );
  LDEBUG( brightness );
  return brightness;
  
}

/*
 * save animation speed to eeprom and exit speed setting mode
 * 
 */
void saveAniSpeed( uint8_t anispeed ) {
  LDEBUG1( "saveAniSpeed called with param=", anispeed );

  EEPROM.update( ANISPEED_ADDR, anispeed );
  
}

/*
 * load animation speed from eeprom
 * 
 */
uint8_t loadAniSpeed( ) {
  LDEBUG0( "loadAniSpeed called. Val=" );
  uint8_t anispeed = EEPROM.read( ANISPEED_ADDR );
  LDEBUG( anispeed );
  return anispeed;
}

/*
 * handle buttons pressed state
 */
void button_pressedCallback(uint8_t pinIn)
{
    //LDEBUG1("PRESS HIGH pin: ", pinIn);

    if( pinIn == BTN1_PIN ) { btn1pressed = true; }
    if( pinIn == BTN2_PIN ) { btn2pressed = true; }
}

/*
 * handle buttons released state
 */
void button_releasedCallback(uint8_t pinIn)
{
    //LDEBUG1("REL LOW pin: ", pinIn);

    if( pinIn == BTN1_PIN ) { btn1released = true; }
    if( pinIn == BTN2_PIN ) { btn2released = true; }
}

/*
 * handle buttons still pressed state
 */
void button_pressedDurationCallback(uint8_t pinIn, unsigned long duration)
{
    //LDEBUG0("HIGH pin: ");
    //LDEBUG0(pinIn);
    //LDEBUG0(" still pressed, duration: ");
    //LDEBUG1(duration, "ms");

/*
    LDEBUG10("HOLD P:", pinIn);
    LDEBUG10(" D:", duration);
    LDEBUG0("|");
*/

    if( pinIn == BTN1_PIN ) { btn1hold = duration; }
    if( pinIn == BTN2_PIN ) { btn2hold = duration; }

}

/*
 * handle buttons released after hold state 
 */
void button_releasedDurationCallback(uint8_t pinIn, unsigned long duration)
{
/*
  LDEBUG("");
  LDEBUG0("REL LOW pin: ");
  LDEBUG0(pinIn);
  LDEBUG0("released, duration: ");
  LDEBUG1(duration, "ms");
*/

  if( pinIn == BTN1_PIN ) { btn1holdreleased = duration; }
  if( pinIn == BTN2_PIN ) { btn2holdreleased = duration; }

}

/*
 * Animate LED: Start on bottom, go to top and bounce back. rinse repeat.
 * 
 * TODO: Brightness Param
 */
void ledAniUpDown(CRGB color) {
  static uint8_t startIndex = 0;
  static uint8_t directionUp = 1;


  if( directionUp ) {
    // Up
    if( startIndex == 0 ) {
      leds = CRGB::Black;
      // set first led on each side to given color
      ledsRear[0] = color;
      ledsFront[NUM_LEDS_SIDE - 1] = color;
      startIndex++;
    }
    else if( startIndex < NUM_LEDS_SIDE ) {
      leds = CRGB::Black;
      ledsRear[startIndex] = color;
      ledsRear[startIndex - 1] = color;
      ledsRear[startIndex - 1].fadeLightBy(64);                         // Dim 25 % => 64/256
      
      ledsFront[NUM_LEDS_SIDE - 1 - startIndex] = color;
      ledsFront[NUM_LEDS_SIDE - 1 - startIndex + 1] = color;
      ledsFront[NUM_LEDS_SIDE - 1 - startIndex + 1].fadeLightBy(64);    // Dim 25 % => 64/256
      
      startIndex++;
    } 
    else {
      directionUp = 0;
      startIndex = 0;
    }
  } 
  else {
    // Down
    if(startIndex == 0) {
      leds = CRGB::Black;
      // set first led on each side to given color
      ledsRear[NUM_LEDS_SIDE - 1] = color;
      ledsFront[0] = color;
      startIndex++;
    }
    else if( startIndex < NUM_LEDS_SIDE ){
      leds = CRGB::Black;
      ledsRear[NUM_LEDS_SIDE - 1 - startIndex] = color;
      ledsRear[NUM_LEDS_SIDE - 1 - startIndex + 1] = color;
      ledsRear[NUM_LEDS_SIDE - 1 - startIndex + 1].fadeLightBy(64);   // Dim 25 % => 64/256
      
      ledsFront[startIndex] = color;
      ledsFront[startIndex - 1] = color;
      ledsFront[startIndex - 1].fadeLightBy(64);                      // Dim 25 % => 64/256
      
      startIndex++;
    } 
    else {
      directionUp = 1;
      startIndex = 0;
    }  
  }
}


/*
 * Animate LED: Simulation of rocket start
 */
void ledAniRocketStart() {
    // Array of temperature readings at each simulation cell
    static byte heat[numleds];

    // Step 1.  Cool down every cell a little
    for( int i = 0; i < numleds; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / numleds) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k = numleds - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    
    if( random8() < SPARKING ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }
    
    
    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < numleds; j++) {
      //CRGB color = HeatColor( heat[j]);

      // Scale the heat value from 0-255 down to 0-240
      // for best results with color palettes.
      byte colorindex = scale8( heat[j], 240);
      CRGB color = ColorFromPalette( rocketPal, heat[j]);
      
      //int pixelnumber;
      //pixelnumber = (numleds - 1) - j; // reverse direction for Rear LEDs
      
      ledsRear[(numleds - 1) - j] = color;  // reverse direction for Rear LEDs (upside down fire)
      ledsFront[j] = color;
      
    }
}

/*
 * plasmac0r's Thruster animation. 
 * Take length of jetwash, firetail, calculate smoke segments, give thruster simulation.
 * 
 *  0  1  2  3  4  5  6  7  8  9  0  1
 * [ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]
 *
 * Jetwash = 5 | Firetail = 2 | Smoke = (12 - 5 - 2) = 5
 *  0  1  2  3  4  5  6  7  8  9  0  1
 * [B][B][B][B][B][Y][Y][W][W][W][W][W]
 * |-------------||----||-------------|
 *     jetwash   firetail    smoke
 *
 * Jetwash = 3 | Firetail = 4 | Smoke = (12 - 3 - 4) = 5
 *  0  1  2  3  4  5  6  7  8  9  0  1
 * [B][B][Y][Y][Y][Y][W][W][W][W][W][W]
 *
 * * Soll eine Palette verwendet werden? Oder direktes Setzen der Farben?
 * * 
 */
void ledAniThruster( int jetwash, int firetail ) {
  CRGB cJetwash   = CRGB(32, 64, 250);    // white/blue
  CRGB cFiretail  = CRGB(255, 150,   0);  // orange/yellow
  CRGB cSmoke     = CRGB(120, 255, 225);  // white
  //int numleds = 12;
  int jetwash_palette_idx;
  int jetwash_pos;
  int firetail_palette_idx;
  int firetail_pos;
  
  static int curPosFiretail = jetwash + 1;          // current position of firetail start
  static int curPosSmoke = jetwash + firetail + 1;  // current position of smoke start
  
  // Randomly change position and length of firetail and smoke a bit
  if (random(1,3) == 1 && curPosFiretail > jetwash - 1 ) {
    curPosFiretail--;
  } else if ( curPosFiretail < jetwash + 2 ) {
    curPosFiretail++;
  }
  
  if (random(1,3) == 1 && curPosSmoke > firetail - 1) {
    curPosSmoke--;
  } else if ( curPosSmoke < firetail + 2 ) {
    curPosSmoke++;
  }
  
  for (int j = 0; j < NUM_LEDS_SIDE; j++) { 
    /*
    jetwash_start = 0;
    jetwash_end = curPosFiretail - 1;
    firetail_start = curPosFiretail;
    firetail_end = curPosSmoke - 1;
    smoke_start = curPosSmoke;
    smoke_end = numleds;
    CRGB cJetwash = ColorFromPalette( jetwashPal, ???? );
    */
  
    if ( j < curPosFiretail) {
      // Segment directly under rocket
      //leds[j] = cJetwash;

      /*
      if (jetwash_pos == 0) { 
        jetwash_palette_idx = 0;
      }
      else if (jetwash_pos == curPosFiretail - 1) {
        jetwash_palette_idx = 255;
      }
      else {
        jetwash_palette_idx = jetwash_pos * (255 / (curPosFiretail - 1));
      }
      */
      jetwash_palette_idx = jetwash_pos * (255 / (curPosFiretail - 1));
      jetwash_pos++;
      CRGB cJetwash = ColorFromPalette( jetwashPal, jetwash_palette_idx );
      
      ledsFront[j] = cJetwash;
      ledsRear[(NUM_LEDS_SIDE - 1) - j] = cJetwash;  // reverse direction for Rear LEDs. Upper LED has index 11.
      
    }
    else if ( j < curPosSmoke ) {
      // Segment under jetwash segment. In the middle between smoke
      //leds[j] = cFiretail;

      firetail_palette_idx = firetail_pos * (255 / (curPosSmoke - 1));
      firetail_pos++;
      CRGB cFiretail = ColorFromPalette( firetailPal, firetail_palette_idx );
      
      ledsFront[j] = cFiretail;
      ledsRear[(NUM_LEDS_SIDE - 1) - j] = cFiretail;  // reverse direction for Rear LEDs. Upper LED has index 11.
    }
    else {
      // Smoke segment
      //leds[j] = cSmoke;
      ledsFront[j] = cSmoke;
      ledsRear[(NUM_LEDS_SIDE - 1) - j] = cSmoke;  // reverse direction for Rear LEDs. Upper LED has index 11.
    }   
  }
}

/*
 * 
 */
void ledAniRainbow(CRGBPalette16 &palette, TBlendType &blending) {
  
    static uint8_t startIndex = 0;
    startIndex++;  /* motion speed */
    uint8_t colorIndex = startIndex;
    
    for( int i = 0; i < NUM_LEDS_SIDE; i++) {
        ledsFront[i] = ColorFromPalette( palette, colorIndex, BRIGHTNESS, blending);
        ledsRear[(NUM_LEDS_SIDE - 1) - i] = ColorFromPalette( palette, colorIndex, BRIGHTNESS, blending);
        colorIndex += 10;
    }
  
}
