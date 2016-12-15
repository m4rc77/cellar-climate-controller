# cellar-climate-controller
Arduino project to control the climate a cellar (e.g. in the loundry room)


##Required arduino libraries
* [https://github.com/adafruit/Adafruit-RGB-LCD-Shield-Library]

* [https://github.com/adafruit/Adafruit_Sensor]

* [https://github.com/adafruit/DHT-sensor-library]


##Required Hardware:
* Arduino Uno R3 (Atmega328)  
[https://www.adafruit.com/product/50 ]

* LCD Shield Kit w/ 16x2 Character Display (I2C)  
[https://www.adafruit.com/products/772]

* 2 x DHT22 temperature-humidity sensor  
[https://www.adafruit.com/products/385]

* Fan e.g. like Papst 4412F/2 GLL 
[https://www.brack.ch/papst-gehaeuseluefter-4412f2-17333]

* Some resistors to connect all the sensors/fans to the arduino. 

* A relais or a some FETs to switch on/off the FAN


##Precondition for the building (cellar) itself
* A hole in the building hull that allows fresh air to be ventilated in by the fan
* a second hole in the building hull that allows the humid air to go out
