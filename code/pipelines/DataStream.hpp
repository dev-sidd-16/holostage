//
// Created by Lei on 7/7/2017.
//

#ifndef PIPELINES_DATASTREAM_H
#define PIPELINES_DATASTREAM_H
#include <sys/socket.h>
#include <android/log.h>
#include <thread>
#include <queue>
#include "TraceTime.hpp"
float XOFF = 800;
float YOFF = 2200;
float ZOFF = -230;

class Frame{
public:
    bool valid = false;
    std::vector<float> points;
};

class Frames{
private:
    std::mutex m_;
    std::queue<Frame*> frames;
    // Used to measure the network performance.
    int frameCounter = 0;
public:

    ~Frames(){
        while(frames.size() > 0){
            delete(frames.front());
            frames.pop();
        }
    }

    Frame * getFrame(){
        std::lock_guard<std::mutex> lock(m_);
        if(frames.size() > 0){
            Frame * frame = frames.front();
            frames.pop();
            return frame;
        }
        return nullptr;
    }

    void saveFrame(Frame * frame){
        std::lock_guard<std::mutex> lock(m_);
        frames.push(frame);
        // delete frames if rendering is too slow
        if(frames.size() > 5){
            frames.pop();
        }
        frameCounter++;
    }

    int getCount(){
        return frameCounter;
    }

};

Frames frames;

struct JitHeader{
    static const int JIT_MAX_DIM_COUNT = 32;
    static const int JIT_MAX_DIM_STRIDE = 32;
    int size;
    int planeCount;
    int type;
    int dimCount;
    int dim[JIT_MAX_DIM_COUNT];
    int dimStride[JIT_MAX_DIM_STRIDE];
    int dataSize;
    bool valid = false;

    static int getInt(char * data, int & offset){
        int value = 0;
        for(int i = 0; i < 4; i++ ){
            value += data[i + offset] << (24 - i * 8);
        }
        offset += 4;
        return value;
    }

    void decode(char * buffer, int &offset){
        size = getInt(buffer, offset);
        planeCount = getInt(buffer, offset);
        type = getInt(buffer, offset);
        dimCount = getInt(buffer,offset);
        for(int i = 0; i < JIT_MAX_DIM_COUNT; i++){
            dim[i] = getInt(buffer, offset);
        }
        for(int i = 0; i < JIT_MAX_DIM_STRIDE; i++){
            dimStride[i] = getInt(buffer, offset);
        }
        dataSize = getInt(buffer, offset);
        valid = true;
    }
};

class JitNetReader{
private:
    // optimization
    const int MAX_BUFFER_SIZE = 500000;
    char * const largeBuffer = new char[MAX_BUFFER_SIZE];

    // read large amount of data,
    // the buffer must be large enough to store the data of the "size"
    int read(int client_socket, char * buffer, int size){
        int bytesRead = 0;
        int result = 0;
        while(bytesRead < size){
            result = recv(client_socket, buffer + bytesRead, size - bytesRead, 0);
            if(result < 1){
                return -1;
            }
            bytesRead += result;
        }
        return bytesRead;
    }

    float getFloat(char * data, int & offset){
        float value;
        char temp[4];
        for(int i = 0; i < 4; i++){
            // consider the endian?
            temp[i] = data[3 - i + offset];
        }
        memcpy(&value, temp, 4);
        offset += 4;
        return value;
    }

    int getInt(char * data, int & offset){
        int value = 0;
        for(int i = 0; i < 4; i++ ){
            value += data[i + offset] << (24 - i * 8);
        }
        offset += 4;
        return value;
    }

    int getIntReverse(char * data, int & offset){
        char * temp = reverse(data, 4, offset);
        int value = 0;
        for(int i = 0; i < 4; i++){
            value += temp[i] << (24 - i * 8);
        }
        offset += 4;
        delete(temp);
        return value;
    }

    char * reverse(char * data, const int size, const int offset){
        char* temp = new char[size];
        for(int i = size -1; i >= 0; i--){
            temp[size - 1 - i] = data[offset + i];
        }
        return temp;
    }

    bool isJMTX(const char * array){
        std::string recv(array, 4);
        if(recv.compare("JMTX") == 0 || recv.compare("XTMJ") == 0){
            return true;
        }
        return false;
    }

public:

    JitNetReader(){
//        this->client_socket = client_socket;
    }

    ~JitNetReader(){
        delete largeBuffer;
    }

    Frame * readFrame(int client_skt){
        char *buffer = largeBuffer;
        int length = read(client_skt, buffer, 4);
        if(length <= 0){
            return nullptr;
        }
        if(isJMTX(buffer)){
            length = read(client_skt, buffer, 4);
            int offset = 0;
            int nextSize = getIntReverse(buffer, offset);
            length = read(client_skt, buffer, nextSize);
            if(length == nextSize && isJMTX(buffer)){
                int offset = 4;
                JitHeader header;
                header.decode(buffer, offset);
                if(!header.valid){
                    return nullptr;
                }
                int dataSize = header.dataSize;
                char * buff;
                if(dataSize <= MAX_BUFFER_SIZE){
                    buff = largeBuffer;
                }else{
                    buff = new char[dataSize];
                }
                length = read(client_skt, buff, dataSize);
//                __android_log_print(ANDROID_LOG_ERROR, "Test","Matrix data size: %d", length);
                Frame * frame = nullptr;
                if(length == dataSize){
                    frame = parseMatrix(header, buff);
                }else{
                    // error
                    __android_log_print(ANDROID_LOG_ERROR, "Test","Wrong matrix data size.");
                }
                // release the local variable
                if(buff != largeBuffer){
                    delete(buff);
                }
                return frame;
            }else{
                // error
            }
        }else{
            // error
        }
        return nullptr;
    }

    Frame * parseMatrix(JitHeader &header, char * buffer){
        int planeCount = header.planeCount;
        int dimCount = header.dimCount;
        int * dim = header.dim;
        int * dimStride = header.dimStride;

        if(dimCount < 1){
            return nullptr;
        }
        switch(dimCount){
            case 1:{
                //
                break;
            }
            case 2:{
                int width = dim[0];
                int height = dim[1];
                Frame * frame = new Frame();
                for(int i = 0; i < height; i++){
                    char * bp = buffer + i * dimStride[1];
                    int offset = 0;
                    for(int j = 0; j < width; j++){
                        float Xvalue = getFloat(bp, offset);
                        float Yvalue = getFloat(bp, offset);
                        float Zvalue = 1.0f-getFloat(bp, offset);

                        Xvalue *= 944.88;
                        Yvalue *= 944.88;
                        Zvalue *= 944.88;
                        Xvalue += XOFF;//800;
                        Yvalue += YOFF;//700;
                        Zvalue += ZOFF;//0;

                        frame->points.push_back(Xvalue);
                        frame->points.push_back(Yvalue);
                        frame->points.push_back(Zvalue);

                    }
                }
                return frame;
            }

            default:{
                return nullptr;
            }

        }// switch

        return nullptr;

    }// parseMatrix

};

class Server{
private:
    int server_skt = -1;
    int client_skt= -1;
    bool running = false;
    int port = 7888;

public:

    bool setupSocket(){
        // sending end
        server_skt = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);


        int option = 1;
        if(setsockopt(server_skt, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1){
            __android_log_print(ANDROID_LOG_ERROR, "Test","Set socket reuse address failed.");
            return false;
        }

        if(setsockopt(server_skt, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option)) == -1){
            __android_log_print(ANDROID_LOG_ERROR, "Test","Set socket reuse port failed.");
            return false;
        }

        sockaddr_in addr_org;
        addr_org.sin_family = AF_INET;
        addr_org.sin_addr.s_addr = htonl(INADDR_ANY);
        addr_org.sin_port = htons(port);
        if(::bind(server_skt, (struct sockaddr *) &addr_org, sizeof(addr_org)) == -1){
            __android_log_print(ANDROID_LOG_ERROR, "Test","Bind to port %d failed", port);
            return false;
        }else{
            __android_log_print(ANDROID_LOG_INFO, "Test","Bind to port %d Success", port);
        }


        int queueSize = 10;
        if(listen(server_skt, queueSize) == -1){
            __android_log_print(ANDROID_LOG_ERROR, "Test","TCP listen to port %d failed", port);
            return false;
        }
        __android_log_print(ANDROID_LOG_INFO, "Test","Begin listening to port %d", port);
        return true;
    }

    void waitForConnection(){
        bool gotConnection = false;
        while(!gotConnection){
            sockaddr_in client_addr;
            socklen_t sin_size;
            client_skt = accept(server_skt, NULL, NULL);
            if(client_skt == -1){
                __android_log_print(ANDROID_LOG_ERROR, "Test","Does NOT get client.");
            }else{
                gotConnection = true;
                __android_log_print(ANDROID_LOG_INFO, "Test","Got client.");
            }
        }
    }

    void sendMsg(){
        send(client_skt, nullptr, 0, 0);
    }

    void recvMsg(JitNetReader &reader){
        // TODO: characterization
//        long startTime = getCurrentTimeMillis();
        Frame * frame = reader.readFrame(client_skt);
        if(frame != nullptr){
            frames.saveFrame(frame);
            // TODO:
//            int time = getCurrentTimeMillis() - startTime;
//            __android_log_print(ANDROID_LOG_ERROR, "Test","Save time: %d", time);
//            saveFPS(time);
        }
    }


    void start(){
        running = true;
        if(setupSocket()){
            waitForConnection();
            JitNetReader reader;
            while(running){
                recvMsg(reader);
            }
        }
    }

    void stop(){
        running = false;
        if(server_skt != -1){
            close(server_skt);
        }
        if(client_skt != -1){
            close(client_skt);
        }
    }

    bool isRunning(){
        return running;
    }

};

Server server;

void startServer(){
    if(!server.isRunning()){
        std::thread t(&Server::start, &server);
        t.detach();
    }
}

void stopServer(){
    server.stop();
}

#endif //PIPELINES_DATASTREAM_H
