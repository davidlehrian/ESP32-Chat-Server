# ESP32 Chat Server
This project is an extension of [Chat Client and Server with Libevent](https://github.com/davidlehrian/ChatClientAndServerWithLibevent) in that this ChatServer is functionally the same as the LibeventChatServer from that project except it runs on an ESP32 microprocessor and incorporates mDNS in order for the server to be found on the locale area network. The ChatClient from [Chat Client and Server with Libevent](https://github.com/davidlehrian/ChatClientAndServerWithLibevent) will communicate with this ChatServer just fine but you will need to connect a terminal to the ESP32 when it launches in order to get the ESP32's ip address that can be used on the command line with ChatClient. 

This project is mainly for me to learn how to communicate with an ESP32 via WiFi from an Android and iPhone and to locate it using mDNS but I thought that this, along with the apps may be useful to someone else out there. These apps are fairly simple but demonstrate how to locate the device using mDNS and open a socket for bi-directional communication. 

There are copious comments in the ChatServer.c file that explain all that is going on. As I mentioned, it is functionally the same as the LibeventChatServer from [Chat Client and Server with Libevent](https://github.com/davidlehrian/ChatClientAndServerWithLibevent) but there is no libevent library for the ESP32 nor is there poll() so this server was written with select(). You can learn about unix socket programming and these two methods, poll() and select() at [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/). I can not recommend this guide highly enough. Please see [Chat Client and Server with Libevenr](https://github.com/davidlehrian/ChatClientAndServerWithLibevent) if you are interested in an example of how to use libevent. 
## Building the project<br>
This project was build using Eclipse 4.30.0.v20231201-0110 and the Espressif plugin v2.12 and ESP-IDF 5.1.2. You can download the entire development environment in an Espressif branded version of Eclipse from [Espressid IDE](https://dl.espressif.com/dl/esp-idf/) and it should work with this example. 

1. Checkout the repository
2. Go into Eclipise and select File->Import and under Espressif select "Existing IDF Project".
3. Select the folder you checked out the repository into and click finish.
4. First you need to Build the project. 
5. Then you can double click 'sdkconfig' and go to 'Example Connection Configuration' to set your WiFi SSID and WiFi Password.
6. Then rebuild the project and flash your ESP32.
7. Launch a terminal window to ensure it is connecting to your local WiFi and obtains an ip address

Then you can use the ChatClient from [Chat Client and Server with Libevent](https://github.com/davidlehrian/ChatClientAndServerWithLibevent) to test it out.
