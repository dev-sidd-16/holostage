//
// Created by Lei on 6/14/2017.
//

#ifndef PIPELINES_PROTOCOL_H
#define PIPELINES_PROTOCOL_H

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <android/log.h>

struct HeaderInfo{
//        char * typeTags;
    int uuid, n, m, p, a, b, plane;
};

class OSCMessage{
private:
    // header info, etc.
//    int * intValues;
    // vertices, colors, UVs, etc.
//    float * floatValues = nullptr;
    std::shared_ptr <float> floatValues;
    HeaderInfo headerInfo;

    std::string getString(char * data, int & offset){
        int length = 0;
        for(int i = offset; data[i] != 0; i++){
            length ++;
        }
        char temp[length];
        for(int i = 0; i < length; i++){
            temp[i] = data[offset + i];
        }
        std::string value(temp, length);
        int padding = (4 - length % 4) %4;
        if(padding == 0){
            // There must be some padding
            padding = 4;
        }
        offset += padding + length;
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

protected:
    bool valid = true;
public:

    OSCMessage(){
        valid = false;
    }

    // copy constructor
//    OSCMessage(const OSCMessage &obj){
//        this->valid = obj.valid;
//        this->headerInfo = obj.headerInfo;
//        this->floatValues = obj.floatValues;
//    }

    OSCMessage(char * msg, int length){
        if(length % 4 == 0){
            int offset = 0;
            std::string label = getString(msg, offset);
            std::string typeTags = getString(msg, offset);
            int strLen = typeTags.length();
            int ints[strLen - 1];
            int intCount = 0;
            float floats[strLen - 1];
            int floatCount = 0;
            for(int i = 1; i < strLen; i++){
                switch(typeTags.at(i)){
                    case 'i':
                        ints[intCount] = getInt(msg, offset);
                        intCount ++;
                        break;
                    case 'f':
                        floats[floatCount] = getFloat(msg, offset);
                        floatCount ++;
                        break;
                    case (char)0:
                        continue;
                    default:
                        continue;
                }
            }

            if(intCount >= 7){
                int temp[intCount];
                for(int i = 0; i < intCount; i++){
                    temp[i] = ints[i];
                }
                headerInfo.uuid = temp[0];
                headerInfo.n = temp[1];
                headerInfo.m = temp[2];
                headerInfo.p = temp[3];
                headerInfo.a = temp[4];
                headerInfo.b = temp[5];
                headerInfo.plane = temp[6];
            }else{
                // error
            }

            if(floatCount > 0){
                floatValues = std::shared_ptr<float>(new float[floatCount]);
                float * values = floatValues.get();
                for(int i = 0; i < floatCount; i++){
                    values[i] = floats[i];
                }
            }

        }else{
            // error
            valid = false;
        }
    }

    HeaderInfo getHeader(){
        return headerInfo;
    }

//    int * getIntValues(){
//        return intValues;
//    }

    float * getFloatValues(){
        return floatValues.get();
    }

    bool isValid(){
        return valid;
    }

    // TODO: remove debug code
    void debug(){
        // Test float value
        int size = headerInfo.m * headerInfo.n * headerInfo.p;
        for(int i = 0; i < size; i++){
            float f = floatValues.get()[i];
            int x = 0;
        }
    }

};

struct RenderingData{
    std::vector<float> positions;
};

class Pack{

private:
    // Get the Message by its plane
    OSCMessage * getMessage(int plane){
        for(int i = 0; i < size; i++){
            if(messages[i].getHeader().plane == plane){
                return &messages[i];
            }
        }
        return nullptr;
    }
public:
    bool full = false;
    int size = 0;
    OSCMessage * messages;

    Pack(int size){
        this->size = size;
        messages = new OSCMessage[size];
    }

    ~Pack(){
        delete (messages);
    }

    void put(int index, OSCMessage &msg){
        if(index >= 0 && index < size){
            messages[index] = msg;
            // check if the message array is full
            int i = 0;
            for(; i < size; i++){
                if(!messages[i].isValid()){
//                    __android_log_print(ANDROID_LOG_ERROR, "Pack","Invalid pack.\n");
                    break;
                }
            }
            if(i == size){
                full = true;
                __android_log_print(ANDROID_LOG_ERROR, "Pack","Pack is full\n");
            }
        }
    }

    RenderingData * getRenderingData(){
        if(size > 0 && full){
            OSCMessage * MsgX = getMessage(0);
            OSCMessage * MsgY = getMessage(1);
            OSCMessage * MsgZ = getMessage(2);
            if(MsgX != nullptr && MsgY != nullptr && MsgZ != nullptr){
                RenderingData * data = new RenderingData();
                HeaderInfo info = MsgX ->getHeader();
                int count = info.m * info.n * info.p;
                for(int i = 0; i < count; i++){
                    data->positions.push_back(MsgX->getFloatValues()[i]);
                    data->positions.push_back(MsgY->getFloatValues()[i]);
                    data->positions.push_back(MsgZ->getFloatValues()[i]);
                }
                return data;
            }
        }
        return nullptr;
    }

};

class MessageGroup{
private:
    // for thread safe
    std::mutex m_;
    std::map<int, Pack*> data;
    std::vector<int> uuids;
    int fullMsgCount = 0;
protected:
public:
    MessageGroup(){
    }

    ~MessageGroup(){
        for(auto it = data.begin(); it != data.end(); it++){
            delete(it->second);
        }
    }

    void decode(char msg[], int length){
        OSCMessage oscMsg(msg, length);
        if(!oscMsg.isValid()){
            return;
        }

//        oscMsg.debug();
        saveMessage(oscMsg);
    }

    void saveMessage(OSCMessage &oscMsg){
        std::lock_guard<std::mutex> lock(m_);

        int uuid = oscMsg.getHeader().uuid;

        // Check if the uuid is in the vector
        int size = uuids.size();
        int i = 0;
        for(; i < size; i++){
            if(uuids[i] == uuid){
                break;
            }
        }
        if(i == size){
            // Does not find
            uuids.push_back(uuid);
        }
//        auto it = std::find(uuids.begin(), uuids.end(), uuid);
//        if(it == uuids.end()){
//            // uuid is not in vector
//            uuids.push_back(uuid);
//        }

        auto it2 = data.find(uuid);

        Pack * pack;
        if(it2 != data.end()){
            // find the uuid
            pack = it2->second;
        }else{
            //doesn't contain the uuid
            pack = new Pack(oscMsg.getHeader().b);
            data.insert(std::pair<int, Pack*>(uuid, pack));
        }
        pack->put(oscMsg.getHeader().a, oscMsg);
        if(pack->full){
            fullMsgCount++;
        }
    }

    Pack * getData(){
        std::lock_guard<std::mutex> lock(m_);

        if(fullMsgCount == 0){
            return nullptr;
        }

        for(auto it = uuids.begin(); it != uuids.end();){
            int uuid = *it;
            it = uuids.erase(it);
            auto item = data.find(uuid);
            if(item != data.end()){
                // find the package which has the uuid
                data.erase(item);
                Pack * pack = item->second;
                if(pack->full){
                    fullMsgCount--;
                    return pack;
                }
            }
        }

        return nullptr;
    }

//    Pack * getPack(){
//        std::lock_guard<std::mutex> lock(m_);
//
//        if(uuids.size() > 0){
//            // get the first uuid
//            int uuid = uuids.at(0);
//
//            auto it = data.find(uuid);
//            if(it != data.end()){
//                // find the package which has the uuid
//                Pack * pack = it->second;
//                if(pack->full){
//                    // remove the first package
//                    uuids.erase(uuids.begin());
//                    data.erase(it);
//                    return pack;
//                }else{
//                    // TODO: Need check if there are full packages behind?
//                    // What if there is something wrong with the first package?
//                }
//            }else{
//                // error, there is no data for the first uuid in the vector
//                // remove the uuid
//                uuids.erase(uuids.begin());
//            }
//
//        }
//
//        // no package or package is not full
//        return nullptr;
//
//    }

};


#endif //PIPELINES_PROTOCOL_H
