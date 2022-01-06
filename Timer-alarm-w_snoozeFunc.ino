/*!
 * Author: William Hak
 * Owner: Mylo Speijers
 * Date:  05-01-22
 * Last update: 06-01-22
 * 
 * General description:
 *    this code is a prototype for a sort of clock
 *    there is a lamp with individual adressable leds type: ws2812
 *    there is a buzzer for the alarm
 *    there are 2 buttons(start, snooze) one will start the clock and the other will snooze the alarm
 *     
 * Clarification and precision:  
 *    the ADC has a precision of 10 bits what means we can get a maximum resolution of 0-1024
 *    so if we have 5 minutes devided over 1024 it result in, 3.4 adc values per second, wich is accurate enough 
 * 
 * for clarity there will be more comments to be clear whats happening in the code.
 */

#include <Adafruit_NeoPixel.h> // for the led ring

//pinDefinitions
#define buttSnooze0_PIN 7
#define buttSnooze1_PIN 8

#define buzzer_PIN 3
#define pot_PIN A0

//leds(WS2812)
#define LED_PIN 10
#define LED_COUNT  16 //or 32 if you connect 2 rings
#define BRIGHTNESS 50

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)


//sounds pitch(TONE) and amount of beeps(CNT) or duration the length of a beep
#define ALARM_TONE 3500
#define ALARM_CNT 4
#define ALARM_TONEINT 1500
#define knobToZero_TONE 2500
#define knobToZero_CNT 1
#define knobChangedfewsec_TONE 3000
#define knobChangedfewsec_TONEdur 5
#define STARTTIMER_CNT 2
#define STARTTIMER_TONE 3000

//advanced
#define BEEP0TRIGGER_ADCAmnt 34
#define TIMESETTRIGGER_ADCAMNT 34
#define delayForAutoStartTimer 2000 // in ms
#define MaxTimerValue_minutes 1UL
#define MaxTimerValue MaxTimerValue_minutes * 60UL
#define MaxTimerValue_ms MaxTimerValue * 1000UL
#define SnoozeTIME 5000 //should be 30000

//COLORS
#define RED_RGB     strip.Color(255, 0, 0)
#define GREEN_RGB   strip.Color(0, 255, 0)
#define BLUE_RGB    strip.Color(0, 0, 255)
#define OCEAN_RGB   strip.Color(0, 255, 255)
#define YELLOW_RGB  strip.Color(255, 255, 0)
#define MAGENTA_RGB strip.Color(255, 0, 255)
#define BLACK_RGB   strip.Color(0,0,0)

//declaration of funcions
bool get_buttonState(short button_PIN);
unsigned int get_rotaryKnobVal(short potPin);
bool beep(short buzz_PIN, unsigned long interval_ms, unsigned long duration_ms, unsigned int freq_hz);
bool knobValChanged(unsigned int amountChanged);
void beepfeedback();
bool updateANDcheckTimerDone();
void ringAlarmSeq();
bool timeBeingSet();
bool rotarynob0();
void set_timer(unsigned long setTime_ms);

//variable settings
bool beepForKnobTurn = true;

unsigned long timerTimeRemaining_T = 0;
unsigned long prevUpdateTimer_T = 0;
bool ringEn = false;

void setup() {
  //ledstrip start
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(BRIGHTNESS);

  
  //initialize the buttons AS INPUTS configured with the internal PULLUP resistor 
    //!(keep in mind, with pullup enabled digital 'High' or 5 volts will be 0 and digital 'Low' will be 1)
  pinMode(buttSnooze0_PIN, INPUT_PULLUP);
  pinMode(buttSnooze1_PIN, INPUT_PULLUP);

  //init potentiometer
  pinMode(pot_PIN, INPUT);

  //init onboard led
  pinMode(LED_BUILTIN, OUTPUT);

  //init buzzer
  pinMode(buzzer_PIN, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println(timerTimeRemaining_T, DEC); // for debugging to see what the timer is at

  timerFlow(); //code for timer logic

  if (beepForKnobTurn == true){ //only for beep feeback like beeping when turning knob etc.. and for turning of the led if the knob is at 0
    beepfeedback();
  }
}

void timerFlow(){
//  timer logic
  static bool timerBeingSet = false;
  static unsigned long timerBeingSet_T = 0;
  
  //see if knob is turned if so save the time and remember that the timer is being set
  if(timeBeingSet()){ 
    timerBeingSet = true;
    timerBeingSet_T = millis();
  }
  
  //auto start Timer after the knob has not been turned for the specified time
  if((millis() - timerBeingSet_T) > delayForAutoStartTimer && timerBeingSet == true && !rotarynob0()){  
    unsigned long knobVal_T = 0;
    knobVal_T = map(get_rotaryKnobVal(pot_PIN), 0, 1024, 0, MaxTimerValue_ms);
    set_timer(knobVal_T);    

    //beep as feedback, the timer has started
    noTone(buzzer_PIN); 
    for(int i = 0; i < STARTTIMER_CNT; i){ //beep specified amount of times   
      if(beep(buzzer_PIN, 150, 100, STARTTIMER_TONE)){
         i++;
      }     
    }
    
    timerBeingSet = false;
    ringEn = true;
  }
  
  static int prevbeeptime_T = 0;
  if(updateANDcheckTimerDone() && ringEn){      
    //checkbuttons for snooze
    //or knob turned back fully for stop, get_rotaryKnobVal
    while(!rotarynob0() && !(get_buttonState(buttSnooze0_PIN) && get_buttonState(buttSnooze1_PIN))){ //if the timer knob is set to 0 or when both buttons are pressed does the alarm stop  }
      ringEn = false;
      if(millis() - prevbeeptime_T > ALARM_TONEINT){  
        prevbeeptime_T = millis();    
        ringAlarmSeq(); // ring the alarm sequence      
      }
    }   

    //if snoozebuttons are pressed
    if((get_buttonState(buttSnooze0_PIN) && get_buttonState(buttSnooze1_PIN))){
      snooze(SnoozeTIME);//add 30 seconds to the timer
    }
  }       
}

bool rotarynob0(){
  if(get_rotaryKnobVal(pot_PIN) < BEEP0TRIGGER_ADCAmnt){//if knob gets turned to zero
    return 1;
  }
  return 0;
}

void set_timer(unsigned long setTime_ms){
      timerTimeRemaining_T = setTime_ms;
      prevUpdateTimer_T = millis();
}

bool updateANDcheckTimerDone(){
  if(timerTimeRemaining_T > (millis() - prevUpdateTimer_T)){//if substracting the time that has passed is still possible from the timer var
                                                           //then the timer is not 0 yet
    timerTimeRemaining_T -= millis() - prevUpdateTimer_T;
    prevUpdateTimer_T = millis();
  }else{
    return 1;//stop timer
  }
  return 0;
}
  
void ringAlarmSeq(){
  //setleds
  strip.fill(GREEN_RGB);
  strip.show();
  
  noTone(buzzer_PIN); 
    
  for(int i = 0; i < ALARM_CNT; i){ //beep specified amount of times   
    if(beep(buzzer_PIN, 150, 100, ALARM_TONE)){
       i++;
    }     
  }
}

void snooze(long time_ms){
  //set leds
  strip.fill(RED_RGB);
  strip.show();

  
  set_timer(time_ms);
  ringEn = true;
}

bool timeBeingSet(){
  static int prevKnobVal_timeset = 0;
  if(knobValChanged(TIMESETTRIGGER_ADCAMNT, prevKnobVal_timeset)){
    prevKnobVal_timeset = get_rotaryKnobVal(pot_PIN);
    return 1;
  }
  return 0;
}

void beepfeedback(){
  static int prevKnobVal_turn10 = 0;
  if(knobValChanged(BEEP0TRIGGER_ADCAmnt, prevKnobVal_turn10)){ //beep everytime more than 10  seconds in corresponding rotational distance has been changed on the knob
    prevKnobVal_turn10 = get_rotaryKnobVal(pot_PIN);
    beep(buzzer_PIN, 0, knobChangedfewsec_TONEdur, knobChangedfewsec_TONE);    
  }

  //beep two times if the knob reached 0 and everything turns of 
  static bool BeepedFor_LT33 = false;
  if(get_rotaryKnobVal(pot_PIN) < 33 && BeepedFor_LT33 == false){
    BeepedFor_LT33 = true;
    
    strip.fill(BLACK_RGB);
    strip.show();
    
    noTone(buzzer_PIN); 
    for(int i = 0; i < knobToZero_CNT; i){ //beep specified amount of times   
      if(beep(buzzer_PIN, 150, 100, knobToZero_TONE)){
         i++;
      }     
    }
    
  }else if (get_rotaryKnobVal(pot_PIN) >= 33){
     BeepedFor_LT33 = false;
  }
}

bool knobValChanged(int amountChanged, int prevPotVal_KVC){
  if(((int)get_rotaryKnobVal(pot_PIN) - prevPotVal_KVC) > amountChanged || ((int)get_rotaryKnobVal(pot_PIN) - prevPotVal_KVC) < -amountChanged ){
    prevPotVal_KVC = get_rotaryKnobVal(pot_PIN);
    return true;
  }
  return false;
}

//core functions
bool get_buttonState(short button_PIN){ //short refers to 1 Byte or 8 bits, instead of int-> 4 Bytes, this is for saving memory
  bool state = !(digitalRead(button_PIN)); //invert because of PULLUP, it caused inverted logic
 
  return state; //return value 1 if button is pressed or 0 if the button is not pressed 
}

unsigned int get_rotaryKnobVal(short knob_Pin){
  unsigned int potReading = analogRead(knob_Pin);//get value from 0-1024 wich corresponds to 0-5 volts
  return potReading;
}

bool beep(short buzz_PIN, unsigned long interval_ms, unsigned long duration_ms, unsigned int freq_hz){
  static unsigned long previous_T = millis(); //make var static to save value even if the program exits the funcion 
                                             //so in the next funcion call we can use the value that was stored in the previous call
  if((millis() - previous_T) > interval_ms && (previous_T - millis()) > duration_ms){
    previous_T = millis();
    tone(buzz_PIN, freq_hz, duration_ms);
    return 1;
  }
  return 0;
}

//LED efects
void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops) {
  int fadeVal=0, fadeMax=100;

  // Hue of first pixel runs 'rainbowLoops' complete loops through the color
  // wheel. Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to rainbowLoops*65536, using steps of 256 so we
  // advance around the wheel at a decent clip.
  for(uint32_t firstPixelHue = 0; firstPixelHue < rainbowLoops*65536;
    firstPixelHue += 256) {

    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255,
        255 * fadeVal / fadeMax)));
    }

    strip.show();
    delay(wait);

    if(firstPixelHue < 65536) {                              // First loop,
      if(fadeVal < fadeMax) fadeVal++;                       // fade in
    } else if(firstPixelHue >= ((rainbowLoops-1) * 65536)) { // Last loop,
      if(fadeVal > 0) fadeVal--;                             // fade out
    } else {
      fadeVal = fadeMax; // Interim loop, make sure fade is at max
    }
  }

  for(int k=0; k<whiteLoops; k++) {
    for(int j=0; j<256; j++) { // Ramp up 0 to 255
      // Fill entire strip with white at gamma-corrected brightness level 'j':
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
    delay(1000); // Pause 1 second
    for(int j=255; j>=0; j--) { // Ramp down 255 to 0
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
  }

  delay(500); // Pause 1/2 second
}
