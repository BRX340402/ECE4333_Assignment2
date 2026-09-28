#pragma once
#include <Arduino.h>
struct FaceBox{int x,y,w,h;};
struct FaceOutput{int count,id,samples,error;FaceBox boxes[4];};
extern bool faceEnabled,faceRecognize,faceEnroll;
void initFaces();
FaceOutput processFaces(const uint8_t *pixels,int w,int h);
