A fusion of Libmapper and Firmata to create signals more easily


What is on this repository :
    - A software part that you have to compile and run and a firmware part divided 
           in a library you have to place in the libraries folder of the Arduino 
           software, and a program to flash your Arduino
    - There are three versions of the previous set :
      	    	 - A standard version for Arduino
		 - An adaptation for Raspberry Pi with only a software part and no analog pin available
		 - A personnalised  adaptation for the T-Stick of Joseph Malloch


How to run :   

    0) I assume that you already have Libmapper and all its dependencies installed on your computer. 
       	   If not, you can see how to do it on the corresponding website :
           http://www.idmil.org/software/libmapper

    1) Create a clone of the following repository on your computer :
       	   https://github.com/RDju/firmata-mapper.git or git@github.com:RDju/firmata-mapper.git

    2) Adjust the Makefile of the software part according to your computer (Linux by default), and compile it

    3) Copy past the Arduino library FirmataLib in the corresponding folder of the Arduino software

    4) Flash your Arduino with the firmware programm Firmapper_firmware.ino

    5) Launch the Firmapper software program and start creating your signals


With this program, you can :

    - Add a new signal (choose a name, a unit, a mode and a pin) and delete it
    - Save your configuration on EEPROM (and load it as long as you don't write something else on it)
    - Save your configuration on an extern file (and load it when you want)
    - Use it on Raspberry Pi with the adapted program
    - Link all your signals with other sensors or signals from any audio program with Webmapper


Warnings :
 	 
    - If you want to use an Atmega168 instead of an Atmega328 you have to change 
      	   the size of the name and the unit from 12 and 5 to 2 and 2
           in firmata_mapper.cpp in the software and Firmata_mapper.h in the library firmware
    - The delete button does not work the 2.9 version of wxWidgets for the moment because 
      	   of an incompatibility with the swig library
    - When you save a configuration, if you want to replace a file, 
      	   do not select the file you want to replace but write its name 
      	   in the text control without the extension ".mapconf"
    - Do not use the RESET button of the Arduino
    - Do not use a save name of only one character, or bigger that 9 characters
    - The adaptation for Raspberry Pi is functionnal but the response time is very slow

Feel free to contact me at julie.rene2@mail.mcgill.ca if you find some bugs !
