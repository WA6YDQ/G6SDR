This is the description for the G6 SDR radio.

As of 10/3/2020 this is very much a work-in-progress. There
is no current schematic, but the defines in the software 
tell how everything is connected.

Everything WILL change as this moves forward!

The front end is a QRP Labs receive module. This is an
inexpensive kit that has an input for the antenna, an
input for the reference oscillator and I and Q outputs.
Think of it as a glorified mixer.

The oscillator is also a QRP Labs si5351 oscillator clock kit.
I've used si5351 clocks from Adafruit in the past and they
work very well. However, the reference clock for the Adafruit
module is 25 MHz and I wanted 27 MHz which the QRP Labs kit
provides.

The processor is currently a Teensy 4.0 controller. I will
be moving to a 4.1 controller in the next few days. The only
difference between the two is the 4.1 brings out all of the pins
to solder points on top where the 4.0 has the solder pads on the 
bottom and is harder to reproduce if you don't have a lot of
SMT experience.

The DAC/ADC is an audio board (using I2S) from the same folks 
that produce the Teensy (pjrc.com). You can use other ways of
getting audio out but this is among the cheapest and works 
very well. I used the 'Audio Shield' for the 4.x boards.

The DSP routines are still in flux - they are working (and much
works quite will) but it's a work in progress.

Update:
The DSP routines I'm using today come from Derek Rowell, but I'm 
having some issues with some things. My own code (which I'll post 
later) works better at some things (filters), but not so good at SSB 
demodulation. Dereks code is included here, but his repository
is at https://github.com/DerekRowell/AudioSDR

All of the controller routines (except the pjrc audio libs) are 
my own code. 

All of the software here is updated as I make major changes. Everything
works now - the updates will also work but might be radically different.

-- Kurt


