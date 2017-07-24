//
// Created by Lei on 6/12/2017.
//

#ifndef PIPELINES_DATAGRAM_H
#define PIPELINES_DATAGRAM_H

#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <unistd.h>
#include <android/log.h>
#include <unistd.h>
#include "protocol.hpp"
#include <thread>

#define MAX_DATA_BUF_LEN 4096

int sd = -1;
int end_flag = 0;
int port = 7999;
MessageGroup msgGroup;
//sockaddr_in addr_dst;

void setupSocket(){
    // sending end
    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);;
    sockaddr_in addr_org;
    addr_org.sin_family = AF_INET;
//    addr_org.sin_addr.s_addr = inet_addr("192.168.0.102");
    addr_org.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_org.sin_port = htons(port);
    ::bind(sd, (struct sockaddr *) &addr_org, sizeof(addr_org));

    // receiving end
//    addr_dst.sin_family = AF_INET;
//    addr_dst.sin_addr.s_addr = inet_addr("192.168.0.107");
//    addr_dst.sin_port = htons(21234);
}

void sendMsg(const void * msg, int length){
//    sendto(sd, msg, length, 0, (struct sockaddr *) &addr_dst, sizeof(addr_dst));
}

void processMsg(char * msg, int length){
//    OSCMessage message(msg, length);
    msgGroup.decode(msg, length);
//    __android_log_print(ANDROID_LOG_ERROR, "Test","message uuid: %d", message.getHeader().uuid);
}

void receiveMsg(){
    
    while (!end_flag) {
//        printf("waiting on port %d\n", 8080);
//        __android_log_print(ANDROID_LOG_ERROR, "Test", "Waiting on port : %d\n", port);
        char buffer[MAX_DATA_BUF_LEN];
//        int len = sizeof(addr_dst);
        int recvlen = recvfrom(sd, buffer, MAX_DATA_BUF_LEN, 0, NULL,
                               NULL);
//        __android_log_print(ANDROID_LOG_ERROR, "Test","received %d bytes\n", recvlen);
        if (recvlen > 0) {
            buffer[recvlen] = 0;
            __android_log_print(ANDROID_LOG_ERROR, "Test","received message: \"%s\"\n", buffer);
            // process the message
            processMsg(buffer, recvlen);
        }
    }

}

void receiveMsgOnThread(){
    std::thread t(receiveMsg);
    t.detach();
}

void startSocket(){
    setupSocket();
    receiveMsgOnThread();
}

void closeSocket(){
    // stop receiving message
    end_flag = 1;
    // close the socket
    if(sd != -1){
        close(sd);
    }
}

#endif //PIPELINES_DATAGRAM_H

