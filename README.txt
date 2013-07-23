Jeopardy game setup with software, hardware and firmware
based on the CCC beopardy project (2010)

The repo contains the following subdirectories:

arduino         - the firmware for the Arduino Nano, controlling the
                  button lamps and signalling the buttons over the 
                  serial port to the PC/laptop running beopardy_2010

beopardy_2010   - the Jeopardy game software, written in Perl+Tk
                  This program displayes the game board and implements
                  the game.

hardware        - the game controller for push buttons and their lamps
