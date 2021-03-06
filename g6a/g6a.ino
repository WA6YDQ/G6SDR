/* g6 Software Defined Radio 
 *  
 *  using teensy 4.x as cpu, Si5351 as osc
 *  qrp labs receiver module, dsp routines for demod/filtering
 *  teensy audio libraries for audio control
 *  
 *  (C) kurt theis 2020
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 *  Libraries and sdr routines not written by me are copyright by
 *  their respective owners. See their code for details.
 *  
 */

// basic radio includes
#include <EEPROM.h>
#include <Encoder.h>        // shaft encoder
#include "si5351.h"         // clock osc
#include <Metro.h>          // interval timer for background routines  
#include <Wire.h>           // I2C for audio/oscillator 
#include "g6.h"             // global variables, pre-defined channels, etc

// audio and sdr includes
#include <Audio.h>
#include <Arduino.h>
#include "AudioSDRlib.h"
#include <SPI.h>




// for sdr and all audio
// --- Audio library classes

AudioInputI2S          IQinput;
AudioAmplifier         amp1;
AudioSDRpreProcessor   preProcessor;
AudioGrabberComplex256 spectrumData;
AudioSDR               SDR;
AudioFilterStateVariable    bpfilter;   // cw filter (enabled in showMode())
AudioOutputI2S         audio_out;
AudioControlSGTL5000   codec;
//---
// Audio Block connections
AudioConnection c1(IQinput, 0,       preProcessor, 0);
AudioConnection c2(IQinput, 1,       preProcessor, 1);
AudioConnection c21(preProcessor,0,spectrumData,0);
AudioConnection c22(preProcessor,0,spectrumData,1);
AudioConnection c3(preProcessor, 0,  SDR, 0);
AudioConnection c4(preProcessor, 1,  SDR, 1);
AudioConnection c51(SDR,0,bpfilter,0);
AudioConnection c52(bpfilter,1,amp1,0); // bandpass, resonance mode
AudioConnection c6(amp1,0,audio_out,0); // no audio amp on right channel












// what kind of display is being used?
#define LCDUSED             // we're using an LCD display
//#define TFTUSED              // we're using a tft display


// general defines
#define DEBOUNCE 70         // switch debounce delay
#define CHANNELMAX 100      // largest channel number
#define CHANNELMIN 1        // smallest channel number
#define FREQMIN 15000UL      // 15 khz
#define FREQMAX 30000000UL   // 30 MHz
#define MAXMODE 5
#define REFOSC 27006700     // actual measured reference xtal (use your own, not mine) 

#ifdef LCDUSED
// wiring for the LCD display
#include <LiquidCrystal.h>
#define RS 34            // lcd rs
#define  E 35            // lcd e
#define D7 36            // lcd D7-D4
#define D6 37
#define D5 38
#define D4 39
LiquidCrystal lcd(RS,E,D4,D5,D6,D7);
#endif

#ifdef TFTUSED
// wiring for tft display
#include <ILI9341_t3.h>
#include <font_Arial.h> // from ILI9341_t3
#define TFT_DC      9       // was 20
#define TFT_CS      10      // was 21
#define TFT_RST    255  // 255 = unused, connect to 3.3V
#define TFT_MOSI    11      // was  7
#define TFT_SCLK    13      // was 14
#define TFT_MISO    12
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);
#endif


// front panel buttons and controls:    short/long press
#define SW1   25        // sw1 button - vfo/mem
#define SW2   27        // sw2 button - sto/rcl
#define SW3   29        // sw3 button - rit/clr
#define SW4   31        // sw4 button - qmem
#define SW5   33        // sw5 button - step/save to zero
#define SW6   28        // sw6 button - cw mode
#define SW7   30        // sw7 button - ssb mode
#define SW8   32        // sw8 button - am mode
#define PULSE 24        // shaft encoder step (channel, menu etc)
#define DIR   26        // shaft encoder direction
#define TUNEP 40        // shaft encoder for tuning (frequency tuning)
#define TUNED 41        // shaft encoder dir

// inputs/control outputs
#define DCVOLTIN 14     // analog in - reads dc voltage
#define PREAMPOUT 2     // line out to enable preamp
#define CWKEY 0         // cw key line 0 
#define DOT 0           // keyer dot
#define PTT 0           // voice PTT
#define DASH            // keyer dash


// all global vars defined in g6.h

// instantiate oscillator
Si5351 si5351;

// shaft encoder for tuning
Encoder shaftEnc(PULSE,DIR);

// read the dc voltmeter every 1.5 seconds
Metro readDC = Metro(1500);

// debugging - show cpu usage every 60 seconds
Metro showUsage(60000);

// Metro showSmeter(125);  // update s-meter 8 times/second

// read/update volume control every 50 msec
elapsedMillis volmsec = 0;





// ------------------------------------------ //
// ----------- ROUTINES START HERE ---------- //
// ------------------------------------------ //

// pre-load eeprom (called from setup()/menu() if conditions met)
void preLoad() {
unsigned int n, address;

    lcd.print("updating eeprom");
    //delay(1000);
    address=0; n=0;
    while (n < 100) {
        EEPROM.put(address,defaultFreq[n]);
        address += sizeof(unsigned long);
        EEPROM.write(address,defaultMode[n]);
        address += sizeof(byte); 
        n++;
    }
    lcd.clear();
    return;
}


// Debugging: typically audio mem usage max is 6, cpu usage is 10%
// show mem/audio usage
void showMemUsage() {
    Serial.print("Audio usage (inst) ");
    Serial.println(AudioMemoryUsage());
    Serial.print("Audio usage (max) ");
    Serial.println(AudioMemoryUsageMax());
    Serial.print("Processor Usage% (max) ");
    Serial.println(SDR.processorUsageMax());
    
}

void showRxLevel(void) {     // show signal levels
    unsigned long sigLevel = 10;
    int slevel = 0, n, i;
    //return;
    if (spectrumData.newDataAvailable()) {
        spectrumData.grab(FFTbuffer);   // get 512 real/complex values
        for (int n=0; n < 512; n++)  
            sigLevel += abs(FFTbuffer[n++]);  // get average of real values
        sigLevel /= 256;
        //Serial.println(sigLevel);    

    }
    lcd.setCursor(0,3);
    lcd.print("                   ");
    slevel = map(sigLevel,1,5000,0,19);
    slevel = constrain(slevel,0,19);
    //Serial.println(slevel);
    lcd.setCursor(0,3);
    for (n=0; n<slevel; n++) lcd.write(0xff);
    for (i=n; i<20; i++) lcd.print(' ');
    showFreq();
    showStep();
    return;
}


#ifdef LCDUSED
// ------------------------------------------------------ //
// ------- routines to update the LCD display ----------- //
// ------------------------------------------------------ //

// ------- show current frequency ---------
void showFreq() {
    unsigned long long oscfreq;
    int mhz = 0L, khz = 0L, hz = 0L;
    char temp[8];
    lcd.setCursor(0,0);
    if (VFO) {      // vfo mode
        lcd.print("VFO ");
    } else {
        lcd.print("MEM ");
    }
    
    if (FREQ < FREQMIN || FREQ > FREQMAX) {
        lcd.print("[BLANK]  ");
    } else {
 
        // show mhz
        mhz = FREQ/1000000;
        if (mhz == 0) {
            lcd.print("  ");   
        } else {
            sprintf(temp,"%2d",mhz);
            lcd.print(temp);
        }

        // show khz
        khz = (FREQ/1000) - (mhz * 1000);
        sprintf(temp,"%03d",khz);
        lcd.print(temp);
        lcd.print(".");
    
        // show hz
        hz = FREQ - (mhz * 1000000) - (khz * 1000);
        sprintf(temp,"%02d",hz/10);
        lcd.print(temp);

        // send freq to si5351 osc (freq wants milliHz, so mult by 100
        // weaver sdr wants 4x frequency (reason for *400ULL)
        oscfreq = ((unsigned long long)FREQ - (unsigned long long)tuningOffset) * 400ULL;
        si5351.set_freq(oscfreq, SI5351_CLK0); // initial value

    }
   
    return;
}


// --------- show current channel ---------
void showChannel() {
    char temp[4];
    lcd.setCursor(14,0);
    lcd.print("CH");
    sprintf(temp," %03d",CHANNEL);
    lcd.print(temp);
    return;
}


// -------- show receive mode ---------
void showMode() {
    lcd.setCursor(0,1);
    lcd.print(MODENAME[MODE]);
    lcd.print(FILTERNAME[FILTER]);
    tuningOffset = SDR.setDemodMode(MODE);  // set the mode in dsp, get the offset
    if (MODE==2 || MODE==3) {
        bpfilter.resonance(3.9);    // cw filter at 750 hz 
        SDR.setOutputGain(1.6);     // compensate for cw filter's gain
    } else {
        bpfilter.resonance(0.0);    // disable cw filter if not on cw mode
        SDR.setOutputGain(3.5);     // default gain w/no cw filter
    }
    return;
}




// ---- Display D.C. Voltage ---------
void showVoltage() {

    
  // read analog pin, divide by constant for true voltage (I use a 10k multiturn pot)
  float VOLT;
  char buf[5];

  VOLT = analogRead(DCVOLTIN)/55.0;    // read DC voltage (use a float here)
  // dtostrf(float var,str len, digits after decimal point, var to hold val)
  dtostrf(VOLT,4,1,buf);  // arduino can't handle floats (WTF? it has a c compiler)
  lcd.setCursor(14,1);
  lcd.print(buf);
  lcd.write("v");  // show as voltage

  return;
}



// --------- show rit on/off, rit frequency ----------
void showRit() {
    char temp[8];
    lcd.setCursor(0,2);
    lcd.print("RIT");
    if (!RIT) {
        lcd.setCursor(4,2);
        lcd.print("OFF   ");
    } else {
        lcd.setCursor(4,2);
        sprintf(temp,"%+04d  ",(unsigned int)RITFREQ);
        lcd.print(temp);
    }
    return;
}


// --------- show step size --------
void showStep() {
    if (STEPSIZE==10L) lcd.setCursor(11,0);
    if (STEPSIZE==100L) lcd.setCursor(10,0);
    if (STEPSIZE==1000L) lcd.setCursor(8,0);
    if (STEPSIZE==10000L) lcd.setCursor(7,0);
    if (STEPSIZE==100000L) lcd.setCursor(6,0);
        
    lcd.cursor();       // enable underline
    return;

}


// ----------- show preamp -----------
void showPreamp() {     // show if preamp is on/off
    lcd.setCursor(14,2);
    if (preampval) {
        lcd.print("PREAMP");
        return;
    } else {
        lcd.print("      ");
        return;
    }
}


// --------- show AGG on/off
void showAGC() {
    char agc_setting[4]={'O','F','M','S'};
    lcd.setCursor(11,2);
    lcd.print("AGC-");
    lcd.print(agc_setting[AGCVAL]);
    
    return;
}


// ----------- show filter settings ---------
void showFilter() {
    return;
}
// -------------------------------------------------- //
// ------------ END OF LCD ROUTINES ----------------- //
// -------------------------------------------------- //
#endif




// --------------------------------------------------
// --------- various receive control routines -------
// --------------------------------------------------

void ritTune(int encval) {
    if (encval == 1) RITFREQ += 10;
    if (encval == 0) RITFREQ -= 10;
    showRit();
    showStep();
    return;
}


// ------- vfo, rit  and channel tuning ---------
void mainTune(int encval) {   // called from interrupt
    //unsigned int address;

    if (VFO) {  // vfo=1, tune vfo
        if (encval == 1) {      // frequency UP
            if (FREQ+STEPSIZE >= FREQMAX) {
                FREQ = FREQMAX;
            } 
            else 
                FREQ += STEPSIZE;
        } else {
            if (FREQ-STEPSIZE <= FREQMIN) {
                FREQ = FREQMIN;
            }
            else 
                FREQ -= STEPSIZE;
        }
        showFreq();
        showStep();
        return;
    }
    
    if (!VFO) { // vfo=0, tune channel
        if (encval > 0) {  // channel UP
            if (CHANNEL == CHANNELMAX) return;
            CHANNEL += 1;
        } else {
            if (CHANNEL == CHANNELMIN) return;
            CHANNEL -= 1;
        }
        recall();
        showChannel();
        showMode();
        showFreq();
        showStep();
        return;
    }
}


// --------- menu -----------
void menu() {
    return;
}



// -------- scan ----------
void scan() {
    return;
}


// ---------- store -----------
void store() {      // save vfo frequency in the current channel memory
    unsigned int address = CHANNEL*5;     // ull is 8 bytes, mode is 1
    // verify good frequency
    if (FREQ < FREQMIN || FREQ > FREQMAX) return;
    EEPROM.put(address,FREQ);               // save current freq in current channel
    address += sizeof(long);                // point to slot after frequency
    EEPROM.put(address,MODE);               // save mode (lsb/cw/etc)
    lcd.setCursor(0,0);
    lcd.print("Chan ");
    lcd.print(CHANNEL);
    lcd.print(" saved");    // on the LCD this extends past the freq display
    delay(1200);
    showFreq();
    lcd.print("  ");    // remove "saved" overflow
    showChannel();
    showStep();
    return; 
}



// ---------  recall -----------
void recall() {     // recall channel freq/mode into active vfo, jump back to vfo
    unsigned int address;

    address = CHANNEL*5;
    EEPROM.get(address,FREQ);               // get frequency
    if (FREQ < FREQMIN || FREQ > FREQMAX) {
        FREQ = 0;
        SDR.setMute(true);
    } else
        SDR.setMute(false);
    address += sizeof(unsigned long);                // point past frequency to mode
    EEPROM.get(address,MODE);               // and get mode
    if (MODE < 0 || MODE > MAXMODE) MODE=1;
    return;
    
}


// -------------------------------------------
// ----------- setup the hardware ------------
// -------------------------------------------
void setup() {      // called once at turn-on
    
    // set up front panel controls
    pinMode(SW1, INPUT_PULLUP);
    pinMode(SW2, INPUT_PULLUP);
    pinMode(SW3, INPUT_PULLUP);
    pinMode(SW4, INPUT_PULLUP);
    pinMode(SW5, INPUT_PULLUP);
    pinMode(PULSE, INPUT_PULLUP);
    pinMode(DIR, INPUT_PULLUP);

#ifdef LCDUSED    
    // initialize lcd 
    lcd.begin(20,4);
    lcd.display();
    lcd.clear();
#endif

#ifdef TFTUSED
    tft.begin();
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setFont(Arial_24);
    //tft.setTextSize(3);
    tft.setCursor(40, 8);
    tft.println("working!");



#endif
    pinMode(CWKEY, INPUT_PULLUP);
    
    // set output lines
    pinMode(PREAMPOUT, OUTPUT);
    digitalWrite(PREAMPOUT,preampval);

    Wire.begin();
    Serial.begin(9600);
    
    delay(200);        // was 2000

    // setup the master clock/synth
    bool i2c_found;
    i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF,REFOSC,0);    // 27mhz ref clock 
    if (!i2c_found) {
        lcd.print("si5351 not found");
        while(true);
    }
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);   // 4ma works just fine too
    // test:set freq to 7.020MHz (multiply hz output by 100ULL to avoid .01hz requirement)
    si5351.set_freq(702000000ULL, SI5351_CLK0); // initial value
    si5351.output_enable(SI5351_CLK0, 1);       // enable receive osc output
    si5351.output_enable(SI5351_CLK1, 0);       // disable #2 osc output
    si5351.output_enable(SI5351_CLK2, 0);       // disable #3 osc output
    

    
    // if eeprom is uninitialized, pre-load freq/mode
    byte eetest = 0;
    eetest = EEPROM.read(1024);
    if (eetest != 0xba) 
        preLoad();
    EEPROM.write(1024,0xba);



    // for SDR
    // --- Initialize AudioSDR
  preProcessor.startAutoI2SerrorDetection();    // IQ error compensation
  // preProcessor.swapIQ(false);        // set to 'true' if your IQ lines are switched
  //
  SDR.setMute(true);            // keep quiet until everything's running
  SDR.enableAGC();              // turn on agc
  AGCVAL = 1;                   // Fast
  SDR.setAGCmode(AGCVAL);      // 0->OFF; 1->FAST; 2->MEDIUM; 3->SLOW
  SDR.disableALSfilter();
  //
  SDR.enableNoiseBlanker();             // can be turned off later
  SDR.setNoiseBlankerThresholdDb(10.0); // 10.0 is a good start value
  //
  SDR.setInputGain(0.7);               // type 1.25, from 0.0 (off) to 10.0 (max)
  SDR.setOutputGain(3.5);               // 
  SDR.setIQgainBalance(1.020);          // change this for your setup
  //
  // --- initial set up  
  tuningOffset = SDR.setDemodMode(USBmode);  // initial mode
  SDR.setAudioFilter(audio2700);        // initial filter set

    // --- Set up the Audio board
  AudioMemory(20);
  AudioNoInterrupts();
  
  codec.inputSelect(AUDIO_INPUT_LINEIN);
  codec.volume(0.8);      // highest w/o distortion. Using analog pot on A15 for volume cx
  codec.lineInLevel(15);  // Set codec input voltage level to most sensitive
  codec.lineOutLevel(13); // Set codec output voltage level to max gain
  codec.enable();
  AudioInterrupts();
  delay(500);
  //

  // cw only bp filter
  bpfilter.frequency(700);  // freq in Hz
  bpfilter.resonance(0.0);  // turn off initially
  
  amp1.gain(0.0);       // initial gain on the amp - use volume control to set
  //SDR.enableALSfilter();  // just decreases audio, not useful to clean up AM
  //SDR.setALSfilterNotch();
  SDR.setMute(false);






}

// ------------------------------------
// ---------- main loop ---------------
// ------------------------------------
void loop() {

    int volKnob;
    float aGain;
    
    Serial.println("Starting");
    CHANNEL = 0;
    recall();       // restore from channel 0 (stored by long press on SW5 (step)
    CHANNEL = 1;
    tuneval = 0;
    // initial display setup
    showMode();     // tuning offset set in showMode() so it goes 1st
    showFreq();     // showFreq sets the frequency in the si5351
    showChannel();
    showVoltage();
    showRit();
    showPreamp();
    showAGC();
    showFilter();
    showStep();     // this must be LAST!

    // enable filter, set value
    SDR.enableAudioFilter();
    SDR.setAudioFilter(audio3300);
    SDR.setMute(false);


    // loop forever testing switches and (if used) TFT touch screen
    // shaft encoders are tested via interrupts
    while (true) {

       // set audio out via vol pot on line A15 
       if (volmsec > 50) {
            // test volune knob
            volKnob = analogRead(15);
            aGain = (float)volKnob/682.0;  
            if (aGain > 1.50) aGain = 1.5;   // allow turning off volume completely
            amp1.gain(1.5-aGain);
            volmsec = 0;
       }

        // test front panel switches
        
        if (digitalRead(SW4)) {      // vfo/mem/sto/rcl switch
            Serial.print("current vfo/mem is "); Serial.println(VFO,DEC);
            delay(550);
            
            if (digitalRead(SW4)) {      // long press
                while (digitalRead(SW4)) delay(DEBOUNCE);
                delay(DEBOUNCE);
                if (VFO==1) {
                    Serial.println("saving to memory");
                    store();       // 1=vfo, 0=channel
                    continue;
                } 
                if (VFO==0) {
                    Serial.println("recalling from memory");
                    recall();
                    VFO=1;         // revert to vfo when recalling
                }
                showMode();
                showFreq();
                showChannel();
                showStep();
                continue;
            }
            
            // short press
            if (VFO==0) {  // switch to vfo
                Serial.print("switching to vfo: VFO is "); Serial.println(VFO,DEC);
                VFO=1;
                showChannel();
                MODE=tempMODE;
                showMode();
                FREQ=tempFREQ;
                showFreq();
                showStep();
                //Serial.print("VFO is now ");Serial.println(VFO,DEC);
                //delay(500);
                continue;
            } 
            if (VFO==1) {    // switch to channel
                Serial.print("switching to mem: VFO is ");Serial.println(VFO,DEC);
                VFO=0;
                tempMODE=MODE;
                tempFREQ=FREQ;
                recall();       // get channel freq/mode
                showMode();
                showFreq();
                showChannel();
                showStep();
                //Serial.print("VFO is now ");Serial.println(VFO,DEC);
                //delay(500);
                continue;
            }
            
        }
        
        
        if (digitalRead(SW2)) {      // mode/menu switch
            //Serial.println("sw4 pressed");
            MODE++;
            if (MODE > 5) MODE = 0;
            // mode is set in showMode()
            showMode();
            showFreq();     // reset freq based on mode change
            showStep();
            if (digitalRead(SW2)) {
                while (digitalRead(SW2)) delay(DEBOUNCE);
                delay(DEBOUNCE);
            }
            //Serial.println("sw4 released");
            continue;
        }
        
        
        if (digitalRead(SW3)) {      // split-rit/preamp switch
            
            Serial.println("sw3 pressed");
            delay(550);
            if (digitalRead(SW3)) {    // long press
                preampval = abs(preampval-1);   // toggle preamp on/off
                digitalWrite(PREAMPOUT,preampval);
                showPreamp();       // display update
                showStep();
                while (digitalRead(SW3)) delay(DEBOUNCE);
                delay(DEBOUNCE);
                continue;
            }
            
            // short press

            
            // testing different functs here
           
            if (AGCVAL) {
                AGCVAL = 0;     // off
                SDR.setAGCmode(AGCVAL);
                showAGC();
                //SDR.disableALSfilter();
                
            } else {
                AGCVAL = 1;     // fast
                SDR.setAGCmode(AGCVAL);
                showAGC();
                //SDR.enableALSfilter();
                //SDR.setALSfilterPeak();
                
            }
            
            continue;
            /*
            if (RIT==0) {
                RIT=1;
                showRit();
                showStep();
                continue;
            }
            if (RIT==1) {
                RIT=0;
                RITFREQ=0L;
                showRit();
                showStep();
                continue;
            }
            */

        }

        // sw4


        // CW KEY
        /*
        if (digitalRead(SW4)==1) {
            sidetoneout.frequency(SIDETONEFREQ);
            sidetoneout.amplitude(SIDETONEVOL);
            digitalWrite(LED,1);
            if (digitalRead(SW4)==1) {
                while (digitalRead(SW4)==1);
            }
            sidetoneout.amplitude(0);
            digitalWrite(LED,0);
            continue;
        }
        */

        if (digitalRead(SW5)==0) {      // step/scan switch
            //Serial.println("sw5 pressed");
            delay(250);
            if (digitalRead(SW5)==0) {  // long press store vfo to chan 0
                byte TCHAN = CHANNEL;
                CHANNEL=0;
                store();
                CHANNEL = TCHAN;
                if (digitalRead(SW5)==0) {
                    while (digitalRead(SW5)==0) delay(DEBOUNCE);
                    delay(DEBOUNCE);
                }
                continue;
            }
            // short press
            STEPSIZE *= 10L;
            if (STEPSIZE > 100000L) STEPSIZE = 10L;
            showStep();
            if (digitalRead(SW5)==0) {
                while (digitalRead(SW5)==0) delay(DEBOUNCE);
                delay(DEBOUNCE);
            }

            continue;
        }


        // test shaft encoder for tuning/channel change
        tuneval = shaftEnc.read();
        if (tuneval >= oldtuneval+4) {
            oldtuneval=tuneval;
            if (RIT==0)
                mainTune(1);
            if (RIT==1)
                ritTune(1);
            Serial.print("tuneval "); Serial.println(tuneval);
            continue;
        }
        if (tuneval <= oldtuneval-4) {
            oldtuneval=tuneval;
            if (RIT==0)
                mainTune(0);
            if (RIT==1)
                ritTune(0);
            Serial.print("tuneval "); Serial.println(tuneval);
            continue;
        }

        if (readDC.check() == 1) {      // timing set at start of code by metro
            showVoltage();
            showStep();
            
        }

        if (showUsage.check() == 1) {   // timing set at start of code by metro
            showMemUsage();
        }

        /*
        if (showSmeter.check() == 1) {  // show signal strength
            showRxLevel();
        }
        */
        continue;
    }
}   // end of loop() and main code. 
