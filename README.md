naomidiag
=========

A diagnostic program that can be run on a SEGA Naomi system. Aims to provide basic diagnostic functionality for calibrating your CRT, testing and adjusting audio, testing joysticks and buttons, verifying PSW1, PSW2 and DIP switches and testing SRAM/EEPROM to verify that it is good. It is possible that additional tests will be added in the future. If you have a test that you would like to add, pull requests are always accepted! The menu is navigable using joystick up/down and start, or using service to move the cursor and test to select an item. You can also use PSW1/PSW2 to navigate if you do not have a JVS IO attached to your Naomi.

If you just want to run this on your naomi, net boot `naomidiag.bin` using your favorite net boot software. If you wish to modify a test or compile from source, first make sure you have https://github.com/DragonMinded/libnaomi set up. Then, activate the libnaomi environment and run `make` to compile a new version. 
