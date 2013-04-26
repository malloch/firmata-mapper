A fusion of Libmapper and Firmata to create signals more easily

To run :

    1) You must have created a new library in arduino with this repository : 
       	   https://github.com/RDju/Firmata_mapper
       	   It is the Firmata library for Arduino with some adds 
       	   (in terms, the Firmata-mapper library will replace the Firmata library)
   
    2) Flash your Arduino with the StandardFirmata-mapper.ino 

    3) Compile the firmata-mapper program (if you are on Mac, 
       change the makefile configuration from Linux to Mac)

    4) Launch the firmata_mapper program


You can :

    - add a new pin (choose a name, a mode and a pin)
    - save your configuration on EEPROM (and load it as long as you don't write something else on it)
    - save your configuration on an extern file (and load it when you want)


Warnings :

    - do not use space in the names of the pins
    - pins are not saved in the EEPROM in the order of creation
    - Extern configuration files are not compatible between two updates
    - Delete does not work on Macs for the moment
    - When you save a configuration, if you want to replace a file, 
      	   do not select the file you want to replace but write its name 
      	   in the text control without the extension ".mapconf"


Feel free to contact me at julie.rene2@mail.mcgill.ca if you find some bugs !
