NaomiDiag
=========

![main menu of naomidiag](/screenshots/mainmenu.png?raw=true "NaomiDiag Main Menu")

A diagnostic program that can be run on a SEGA Naomi system. Aims to provide basic diagnostic functionality for calibrating your CRT, testing and adjusting audio, testing joysticks and buttons, verifying PSW1, PSW2 and DIP switches and testing SRAM/EEPROM to verify that it is good. It is possible that additional tests will be added in the future. If you have a test that you would like to add, pull requests are always accepted! The menu is navigable using joystick up/down and start, or using service to move the cursor and test to select an item. You can also use PSW1/PSW2 to navigate if you do not have a JVS IO attached to your Naomi.

If you just want to run this on your naomi, net boot `naomidiag.bin` using your favorite net boot software. If you wish to modify a test or compile from source, first make sure you have https://github.com/DragonMinded/libnaomi set up. Then, activate the libnaomi environment and run `make` to compile a new version. 

Monitor Tests
-------------

![monitor tests](/screenshots/video.png?raw=true "NaomiDiag Monitor Tests")

Provides a simple cross hatch, a better variant of the gradient screen than the one built in to the Naomi BIOS, and red/green/blue/white screens to verify purity and white balance.

Audio Tests
-----------

![audio tests](/screenshots/audio.png?raw=true "NaomiDiag Audio Tests")

Plays audio continuously through either the left, right or both speakers for testing various audio-related isssues with your cabinet.

JVS Digital Input Tests
-----------------------

![jvs digital input tests](/screenshots/digital.png?raw=true "NaomiDiag JVS Digital Input Tests")

Provides a way to test the input of both 1P and 2P controls as well as the cabinet's test and service buttons. Also includes a history graph to help you track down buttons that are sticking, sluggish or possibly double-tapping.

JVS Analog Input Tests
----------------------

![jvs analog input tests](/screenshots/analog.png?raw=true "NaomiDiag JVS Analog Input Tests")

Provides the current value as well as the full range of values seen for each analog input available. Displays both joystick view as well as raw analog axis view more appropriate for driver cabinets.

Filter Board Input Tests
------------------------

![filter board input tests](/screenshots/filter.png?raw=true "NaomiDiag Filter Board Input Tests")

Displays the current state of the DIP switches as well as PSW1/PSW2 on the filter board.

EEPROM Tests
------------

![eeprom tests](/screenshots/eeprom.png?raw=true "NaomiDiag EEPROM Tests")

Verifies that the EEPROM can be read from and written to, and that the value stored is retrievable.

SRAM Tests
----------

![sram tests](/screenshots/sram.png?raw=true "NaomiDiag SRAM Tests")

Verifies that the attached SRAM is fully functional, including stuck address and data line tests and a device test to verify that the memory itself is good.
