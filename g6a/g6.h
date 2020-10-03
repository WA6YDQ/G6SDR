/* g6.h  header files for the g6 series sdr
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
unsigned long FREQ = 14065000L;     // frequency in hz
unsigned long tempFREQ = FREQ;      // hold freq while in mem mode
unsigned long IFFREQ = 0L;          // IF frequency offset set by filters
unsigned long STEPSIZE = 10L;       // step size in hz
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
unsigned long long RITFREQ = 0L;      // rit frequency +/- offset to FREQ

// misc 
float voltage = 0.0;        // initialize variable to hold a/d result
int preampval = 0;          // rf preamp 1=on, 0=off
long int tuneval = 0l;      // shaft encoder
int oldtuneval = 0;

// cw sidetone
unsigned int SIDETONEFREQ=750;
float SIDETONEVOL=0.5;

int bypass=0;




// initialize memory channels
const unsigned long defaultFreq[100] = {
    
	10000000l,		 // chan 0,  last freq used
	1830000l,		 // chan 1   160M
	3535000l,		 // chan 2   80M
	7035000l,		 // chan 3   40M
	10110000l,		 // chan 4   30M
	14045000l,		 // chan 5   20M
	18075000l,		 // chan 6   17M
	21045000l,		 // chan 7   15M
	24900000l,		 // chan 8   12M
	28030000l,		 // chan 9   10M
    50110000l,       // chan 10  6M

    2500000l,        // chan 11  wwv 2.5 mc
    5000000l,        // chan 12  wwv 5 mv
    10000000l,       // chan 13  wwv 10 mc
    15000000l,       // chan 14  wwv 15 mc
    3330000l,        // chan 15  CHU 3 mc 
    7850000l,        // chan 16  CHU 7 mc
    14670000l,       // chan 17  CHU 14 mc

    3485000l,        // chan 18  volmet US, Can
    6604000l,        // chan 19  volmet ""  ""
    10051000l,       // chan 20  volmet ""  ""
    13270000l,       // chan 21  volmet ""  ""
    6754000l,        // chan 22  volmet Trenton
    15034000l,       // chan 23  volmet   ""
    3413000l,        // chan 24  volmet Europe
    5505000l,        // chan 25  volmet   ""
    8957000l,        // chan 26  volmet   ""
    13270000l,       // chan 27  volmet   ""
    5450000l,        // chan 28  volmet Europe
    11253000l,       // chan 29  volmet   ""
    4742000l,        // chan 30  volmet Europe
    11247000l,       // chan 31  volmet   ""
    6679000l,        // chan 32  volmet Oceania
    8828000l,        // chan 33  volmet   ""
    13282000l,       // chan 34  volmet   ""
    6676000l,        // chan 35  volmet SE Asia
    11387000l,       // chan 36  volmet   ""
    3458000l,        // chan 37  volmet   ""
    5673000l,        // chan 38  volmet   ""
    8849000l,        // chan 39  volmet   ""
    13285000l,       // chan 40  volmet   ""
    
    5000000l,        // chan 41 placeholder
    5000000l,        // chan 42
    5000000l,        // chan 43
    5000000l,        // chan 44
    5000000l,        // chan 45
    5000000l,        // chan 46
    5000000l,        // chan 47
    5000000l,        // chan 48
    5000000l,        // chan 49
    5000000l,        // chan 50
    5000000l,        // chan 51
    5000000l,        // chan 52
    5000000l,        // chan 53
    5000000l,        // chan 54
    5000000l,        // chan 55
    5000000l,        // chan 56
    5000000l,        // chan 57
    5000000l,        // chan 58
    5000000l,        // chan 59
    5000000l,        // chan 60
    
    5330500l,        // chan 61 60M chan 1 USB
    5346500l,        // chan 62 60M chan 2 USB
    5366500l,        // chan 63 60M chan 3 USB
    5371500l,        // chan 64 60M chan 4 USB
    5403500l         // chan 65 60M chan 5 USB

      
};

const byte defaultMode[100] = {     // "LSB","USB","CWL","CWU"," AM","SAM"
    2,2,2,2,2,2,2,2,2,2,1,  // ham cw bands
    4,4,4,4,4,4,4,          // time signals
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  // volmet
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,        // placeholder
    1,1,1,1,1,                                      // 60M ham ch1-5 USB
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,        // placeholder 66-85
    0,0,0,0,0,0,0,0,0,0,0,0,0,0                     // placeholder 86-99
};
