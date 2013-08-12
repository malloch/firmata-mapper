A fusion of Libmapper and Firmata to create signals more easily

To run :

    0) I assume that you already have Libmapper installed on your computer. 
       	   If not, you can see how to di it on the corresponding website 
           http://www.idmil.org/software/libmapper

    1) You must have created a new library in arduino with this repository : 
       	   https://github.com/RDju/Firmata_mapper
       	   It is the Firmata library for Arduino with some adds 
       	   (Later, this Firmata_mapper library will replace the Firmata library)
   
    2) Flash your Arduino with the StandardFirmata-mapper.ino 

    3) Compile the firmata-mapper program (if you are on Mac, 
       change the makefile configuration from Linux to Mac)

    4) Launch the firmata_mapper program


You can :

    - Add a new signal (choose a name, a unit, a mode and a pin) and delete it
    - Save your configuration on EEPROM (and load it as long as you don't write something else on it)
    - Save your configuration on an extern file (and load it when you want)
    - Use it on Raspberry Pi with the adapted program


Warnings :
 	 
    - If you want to use an Atmega168 instead of an Atmega328 you have to change 
      	   the size of the name and the unit from 12 and 5 to 2 and 2
           in firmata_mapper.cpp in the software and Firmata_mapper.h in the library firmware
    - The delete button does not work the 2.9 version of wxWidgets for the moment
    - When you save a configuration, if you want to replace a file, 
      	   do not select the file you want to replace but write its name 
      	   in the text control without the extension ".mapconf"
    - Do not use the RESET button of the Arduino
    - Do not use a save name of only one character, or bigger that 9 characters
    - The adaptation for Raspberry Pi is functionnal but the response time is very slow

Feel free to contact me at julie.rene2@mail.mcgill.ca if you find some bugs !
