# esp32-webserver
esp32 webserver for turning on and off digital pins for a certain amount of time

---

## installation

to install the WIFI libary and esptool to upload.


---
## use

the ESP32 will boot in to AP mode
connect to the access point ESP_32_accessPoint
open a web browser and type `192.168.4.1` in to the browser
when promted to login use:
    username : `user`
    password : `pass`

then you should be able to control the state of pins 26 and 27

*note `on for hour` only turn the pin on for 10secs*

---
### to

- [ ] change pin state in Timed_Pin_t to enum, to be more semantic
- [ ] add code to dynamically set buttons for number of initialised pins
