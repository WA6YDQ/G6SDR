/* g6.h  header files for the g6 series sdr
 *  
 *  (C) k theis 9/2020
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
*/


// global variables

// frequency and tuning
byte VFO = 1;                       // 1=vfo, 0=memory channel
unsigned long FREQ = 14065000UL;     // frequency in hz
unsigned long tempFREQ = FREQ;      // hold freq while in mem mode
unsigned long IFFREQ = 0UL;          // IF frequency offset set by filters
unsigned long STEPSIZE = 10UL;       // step size in hz
int32_t tuningOffset = 0;           // returned from selectDemodMode(), subtract from FREQ
byte CHANNEL = 1;                   // channel number 1-100

// modes used in dsp/displayed on screen
char MODENAME[9][4] = {"LSB","USB","CWL","CWU"," AM","SAM"}; // matches dsp routines
byte MODE = 1;
byte tempMODE = MODE;

// filters
char FILTERNAME[2][8] = {" NARROW"," WIDE"};
byte FILTER = 1;

// rit/split
int RIT = 0;           // 1=on, 0=off
unsigned long RITFREQ = 0UL;      // rit frequency +/- offset to FREQ

// misc 
float voltage = 0.0;        // initialize variable to hold a/d result
int preampval = 0;          // rf preamp 1=on, 0=off
long int tuneval = 0L;      // shaft encoder
int oldtuneval = 0;

// cw sidetone
unsigned int SIDETONEFREQ=774;      // the CW filter has a center freq of 774 hz
float SIDETONEVOL=0.5;

int bypass=0;

// AGC mode (0=off, 1=fast, 2=medium, 3=slow)
byte AGCVAL;

// S-meter 
int16_t FFTbuffer[512];


// initialize memory channels
const unsigned long defaultFreq[100] = {
    
	10000000UL,		 // chan 0,  last freq used
	1830000UL,		 // chan 1   160M
	3535000UL,		 // chan 2   80M
	7035000UL,		 // chan 3   40M
	10110000UL,		 // chan 4   30M
	14045000UL,		 // chan 5   20M
	18075000UL,		 // chan 6   17M
	21045000UL,		 // chan 7   15M
	24900000UL,		 // chan 8   12M
	28030000UL,		 // chan 9   10M
    27500000UL,      // chan 10  11M
    2500000UL,        // chan 11  wwv 2.5 mc
    5000000UL,        // chan 12  wwv 5 mv
    10000000UL,       // chan 13  wwv 10 mc
    15000000UL,       // chan 14  wwv 15 mc
    3330000UL,        // chan 15  CHU 3 mc 
    7850000UL,        // chan 16  CHU 7 mc
    14670000UL,       // chan 17  CHU 14 mc
    3485000UL,        // chan 18  volmet US, Can
    6604000UL,        // chan 19  volmet ""  ""
    10051000UL,       // chan 20  volmet ""  ""
    13270000UL,       // chan 21  volmet ""  ""
    6754000UL,        // chan 22  volmet Trenton
    15034000UL,       // chan 23  volmet   ""
    3413000UL,        // chan 24  volmet Europe
    5505000UL,        // chan 25  volmet   ""
    8957000UL,        // chan 26  volmet   ""
    13270000UL,       // chan 27  volmet   ""
    5450000UL,        // chan 28  volmet Europe
    11253000UL,       // chan 29  volmet   ""
    4742000UL,        // chan 30  volmet Europe
    11247000UL,       // chan 31  volmet   ""
    6679000UL,        // chan 32  volmet Oceania
    8828000UL,        // chan 33  volmet   ""
    13282000UL,       // chan 34  volmet   ""
    6676000UL,        // chan 35  volmet SE Asia
    11387000UL,       // chan 36  volmet   ""
    3458000UL,        // chan 37  volmet   ""
    5673000UL,        // chan 38  volmet   ""
    8849000UL,        // chan 39  volmet   ""
    13285000UL,       // chan 40  volmet   ""   
    5000000UL,        // chan 41 placeholder
    5000000UL,        // chan 42
    5000000UL,        // chan 43
    5000000UL,        // chan 44
    5000000UL,        // chan 45
    5000000UL,        // chan 46
    5000000UL,        // chan 47
    5000000UL,        // chan 48
    5000000UL,        // chan 49
    5000000UL,        // chan 50
    5000000UL,        // chan 51
    5000000UL,        // chan 52
    5000000UL,        // chan 53
    5000000UL,        // chan 54
    5000000UL,        // chan 55
    5000000UL,        // chan 56
    5000000UL,        // chan 57
    5000000UL,        // chan 58
    5000000UL,        // chan 59
    5000000UL,        // chan 60    
    5330500UL,        // chan 61 60M chan 1 USB
    5346500UL,        // chan 62 60M chan 2 USB
    5366500UL,        // chan 63 60M chan 3 USB
    5371500UL,        // chan 64 60M chan 4 USB
    5403500UL         // chan 65 60M chan 5 USB     
};

const byte defaultMode[100] = {     // "LSB","USB","CWL","CWU"," AM","SAM"
    2,2,2,2,2,2,2,2,2,2,1,  // ham cw bands
    1,1,1,1,1,1,1,          // time signals
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  // volmet
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,        // placeholder
    1,1,1,1,1,                                      // 60M ham ch1-5 USB
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,        // placeholder 66-85
    0,0,0,0,0,0,0,0,0,0,0,0,0,0                     // placeholder 86-99
};
