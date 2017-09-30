#! /bin/bash

sudo gcc server.c messagePacker.c sqlite3_userInfo.c userList.c -o server -lpthread -l sqlite3 -g
sudo gcc user.c messagePacker.c userList.c -o user -lpthread -g

sudo gcc udp_server.c -o udp_server -g
sudo gcc udp_client.c -o udp_client -g

gcc rtsp_receiver.c -o rtsp_receiver $(pkg-config --cflags --libs gstreamer-1.0)
sudo gcc rtsp_sender.c -o rtsp_sender $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtsp-server-1.0)

sudo cp server user udp_server udp_client rtsp_sender rtsp_receiver ./app
