//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com
#include "Core/hash.h"
#include "Core/node.h"
#include "Gui/knob.h"
void Hash::computeHash(){

    boost::crc_optimal<64,0x42F0E1EBA9EA3693,0,0,false,false> result;
    U64* data=(U64*)malloc(sizeof(U64)*node_values.size());

    for(U32 i=0;i<node_values.size();i++){

        data[i]=node_values[i];
    }
    result.process_bytes(data,node_values.size()*sizeof(U64));
    hash=result.checksum();
    free(data);
}
void Hash::reset(){
    node_values.clear();
    hash=0;
}


void Hash::appendNodeHashToHash(U64 hashValue){
        node_values.push_back(hashValue);
}

void Hash::appendKnobToHash(Knob* knob){
    std::vector<U64> values= knob->getValues();
    for(U32 i=0;i<values.size();i++){
        node_values.push_back(values[i]);
    }
}

void Hash::appendQStringToHash(QString str){
    for(int i =0 ; i< str.size();i++){
        node_values.push_back((U64)str.at(i).unicode());
    }
}


