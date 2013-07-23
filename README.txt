Jeopardy game setup with software, hardware and firmware
based on the CCC beopardy project (2010).

Instead of the original CCC's microcontroller, this project uses an
Arduino Nano to control the buttons and lamps. The serial protocol
has been extended to indicate exactly at all times during the game
which players may buzz in and when.

The repo contains the following subdirectories:

arduino         - the firmware for the Arduino Nano, controlling the
                  button lamps and signalling the buttons over the 
                  serial port to the PC/laptop running beopardy_2010

beopardy_2010   - the Jeopardy game software, written in Perl+Tk
                  This program displayes the game board and implements
                  the game.

hardware        - the game controller for push buttons and their lamps


References:

The old 2006 post by Sec:
http://blogmal.42.org/misc/beopardy-2006.story

The 2010 update by Sec:
http://blogmal.42.org/misc/beopardy-2010.story

The new web based beopardy by Sec:
https://github.com/Sec42/beopardy
