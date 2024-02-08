/* 
ChatServer is available for use under the following license, 
commonly known as the 3-clause (or "modified") BSD license:

==============================
Copyright (c) 2023 David Lehrian <david@lehrian.com>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
==============================

This example is for setting up the ESP32 as a simple chat server. It opens a port 8584
and listens for connections. It uses select() library to manage the open connections
and anything it receives it broadcasts to all connected clients. It uses mDNS to
register itself to the network so it can be found by the name "ChatClient" on the local
network. There is an Android and iPhone Chat Client that connect to this server via mDNS.
However, the Windows and Linux based Chat Client require you input the IPv4 address on the
command line to connect. The IPv4 address is printed to the terminal when the ESP32 connects
to the WiFi router. You need to update the sdkconfig to have the WiFi name and password. If 
the server does not receive input from any client for 60 seconds it times out and send a 
message to all clients asking if they are still there. It was created as a way to play 
with sockets on the ESP32 and should not be considered a production product. There is no
encryption of communication provided.

*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <sys/select.h>
#include "mdns.h"

#define PORT                        "8584"
#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3
#define IPV4
//#define IPV6

static const char *TAG = "example";

static void chat_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;

	fd_set master,read_fds;
	struct addrinfo hints, *res;
	int listener, newfd, fdmin, fdmax;
    struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
	struct timeval tv;
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	
	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	// load address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me
	
	int err = getaddrinfo(NULL, PORT, &hints, &res);

	// make a socket, bind it, and listen on it:
	listener = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	err = bind(listener, res->ai_addr, res->ai_addrlen);
	if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
	// free addressinfo as we are done with is
    freeaddrinfo(res);
  
	// create the listener with room for 10 backlog connection requests
	err = listen(listener, 10);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket listening");
    
    // add the listener to the master set for select()
    FD_SET(listener, &master);
    // keep track of the biggest and smallest file descriptor
	fdmax = listener;
	fdmin = listener;
    
    //initialize mDNS service
    esp_err_t erro = mdns_init();
    if (erro) {
		ESP_LOGE(TAG,"MDNS Init failed: %d\n", erro);
        goto CLEAN_UP;
    }

    //set hostname on the ESP32
    ESP_ERROR_CHECK(mdns_hostname_set("esp32-mdns"));
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", "esp32-mdns");

    //add our services
    mdns_service_add("ChatServer", "_ChatServer", "_tcp", 8584, NULL, 0);

	// loop and select() to see when a socket has something ready to read
    while (1) {
		// take a copy of master that gets updated via the select function
		read_fds = master; 
		// select requires the first parameter to be the largest file descriptor + 1, the second is the 
		// set of read_fds that we are wanting to readm the third and fourth are null because we are not
		// looking at write or except fds and the last parameter is the time out. If this value is null
		// it will wait indefinately, if 0 it won't wait at all and immediately return. The value in this
		// example is set to 60 seconds.
		if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
			perror("select");
			exit(4);
		}

		// if found is still false after looking through all the filedescriptors in read_fds
		// then we got here because the select timed out so we send a message to all the 
		// chat clients asking if they are still there
		bool found = false;
		// run through the existing connections looking for data to read
		// this loop is optimized to start at the smallest fdmin and go up through fdmax
		for(int i = fdmin; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { 
				// we got one, set found to true and process it
				found = true;
				if (i == listener) {
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);

					if (newfd == -1) {
						perror("accept");
					} else {
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax) fdmax = newfd; // keep track of the max
						if (newfd < fdmin) fdmin = newfd; // keep track of the min
						// Set tcp keepalive option. This comes directly from the ESP32 socket example.
					    setsockopt(newfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
					    setsockopt(newfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
					    setsockopt(newfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
					    setsockopt(newfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
						ESP_LOGI(TAG, "selectserver: new connection from %s on socket %d\n",inet_ntoa_r(((struct sockaddr_in *)&remoteaddr)->sin_addr, addr_str, sizeof(addr_str) - 1), newfd);
					}
				} else {
					// if it is not the listener then handle data from a client
					char buf[256]; // buffer for client data
					int nbytes;
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						// got error or connection closed by client
						if (nbytes == 0) {
							// connection closed
							ESP_LOGI(TAG,"selectserver: socket %d hung up\n", i);
						} else {
							ESP_LOGE(TAG,"recv");
						}
						close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					} else {
						// we got some data from a client to loop through everyone and send it to them
						for(int j = fdmin; j <= fdmax; j++) {
							// send to everyone
							if (FD_ISSET(j, &master)) {
								// except the listener and ourselves
								if (j != listener && j != i) {
									if (send(j, buf, nbytes, 0) == -1) {
										ESP_LOGE(TAG,"send");
									}
								}
							}
						}
					}
				}
			}
		}
		// if not found send message to everyone except listener 
		if (!found){
			// timed out so see if people are still there
			for(int j = fdmin; j <= fdmax; j++) {
				// send hello?? to everyone
				if (FD_ISSET(j, &master)) {
					// except the listener and ourselves
					if (j != listener) {
						if (send(j, "hello??? are you still there???\n", sizeof("hello??? are you still there???\n") - 1, 0) == -1) {
							ESP_LOGE(TAG,"send");
						}
					}
				}
			}
		}
    }
CLEAN_UP:
    close(listener);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

#ifdef IPV4
    xTaskCreate(chat_server_task, "chat_server", 4096, (void*)AF_INET, 5, NULL);
#endif
#ifdef IPV6
    xTaskCreate(chat_server_task, "chat_server", 4096, (void*)AF_INET6, 5, NULL);
#endif
}
