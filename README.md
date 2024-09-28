# esp32-softap-provision
An ESP32 Arduino example demonstrating using stored Preferences, a builtin AP and Webserver to perform initial setup

Hopefully this example code helps you if you want to:

- Not save static WiFi credentials in your code
- Leave assigning the WiFi network to the end user
- Avoid reflashing if you need to use a different wireless network

We've all experienced the 'Setup' process in most IoT type devices. You power it on and the first thing you need to do is configure it to connect to your wireless network.

That's exactly what this code does. It also (as an example) lets you specify a 'devicename' which is how it might be known on your network once connected.

This code will advertise the devicename using multicastdns once connected so you should be able to ping '<devicename>.local'

TODO: Add WiFI scanning instead of manual SSID entry

TODO: Add ability to completely erase preferences
