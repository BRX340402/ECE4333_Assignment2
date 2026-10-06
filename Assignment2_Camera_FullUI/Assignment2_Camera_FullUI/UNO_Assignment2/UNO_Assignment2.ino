#include <Arduino.h>
#include <Servo.h>
#include <FastLED.h>
#include "IRremote.h"
#include "ArduinoJson-v6.11.1.h"
#include "Hardware.h"
#include "Logic.h"
Servo pan;CRGB led[1];IRrecv remote(IR_PIN);decode_results irResult;
VisionState vision;
// mode:0 standby,1 floor-sensor tracking,2 ultrasonic avoidance,3 range following,
// 4 floor tracking with AI stop guard,5 manual/rocker.
uint8_t mode=0,reason=0;int speedSetting=90,manualDir=9,panAngle=90;
bool aiGuard=false,remoteOwned=false,irOwned=false;uint32_t lastRemote=0,lastTelemetry=0;
int lineL=0,lineM=0,lineR=0,rangeCm=-1,batteryMv=0,leftPWM=0,rightPWM=0;
char input[192];uint8_t inputPos=0;bool receiving=false;
uint8_t avoidPhase=0;uint32_t avoidAt=0;int rangeLeft=-1,rangeRight=-1;
void setPan(int angle){panAngle=constrain(angle,0,180);pan.write(panAngle);}
void motorOutput(int left,int right){
 left=constrain(left,-DRIVE_LIMIT,DRIVE_LIMIT);right=constrain(right,-DRIVE_LIMIT,DRIVE_LIMIT);
 digitalWrite(MOTOR_STBY,(left||right)?HIGH:LOW);
 digitalWrite(MOTOR_LEFT_DIR,left>=0?HIGH:LOW);digitalWrite(MOTOR_RIGHT_DIR,right>=0?HIGH:LOW);
 analogWrite(MOTOR_LEFT_PWM,abs(left));analogWrite(MOTOR_RIGHT_PWM,abs(right));leftPWM=left;rightPWM=right;
}
void standby(){irOwned=false;mode=0;manualDir=9;avoidPhase=0;remoteOwned=false;motorOutput(0,0);}
void chooseMode(uint8_t m,bool fromRemote){
 motorOutput(0,0);irOwned=false;mode=m;manualDir=9;avoidPhase=0;remoteOwned=fromRemote;lastRemote=millis();
 if(m>=1&&m<=4)setPan(90);
 if(m==4)aiGuard=true;
}
char txBuffer[180];uint8_t txPos=0,txLen=0;
void telemetry(){
 if(txPos<txLen)return;
 txLen=snprintf(txBuffer,sizeof(txBuffer),"{\"N\":210,\"M\":%u,\"B\":%u,\"G\":%u,\"T\":%u,\"U\":%d,\"L\":%d,\"C\":%d,\"R\":%d,\"V\":%d,\"S\":%d,\"Q\":%u,\"A\":%d,\"D\":%d}\n",
  mode,reason,aiGuard,vision.latched,rangeCm,lineL,lineM,lineR,batteryMv,panAngle,vision.seq,leftPWM,rightPWM);
 txPos=0;
}
void pumpTelemetry(){
 int available=Serial.availableForWrite();
 while(txPos<txLen&&available-->0)Serial.write(txBuffer[txPos++]);
}
int8_t pendingReply=-1;
void reply(bool ok){pendingReply=ok?1:0;}
void command(char *text){
 StaticJsonDocument<384> doc;
 if(deserializeJson(doc,text))return;
 if(!doc["N"].is<int>())return;
 int n=doc["N"],d=doc["D1"]|0;
 if(n==200){
  const char *keys[]={"S","A","V","P","R","X","Y","W","H"};
  for(uint8_t i=0;i<9;i++)if(!doc[keys[i]].is<long>())return;
  long seq=doc["S"],a=doc["A"],v=doc["V"],p=doc["P"],dist=doc["R"],x=doc["X"],y=doc["Y"],w=doc["W"],h=doc["H"];
  if(seq<0||seq>65535||a<0||a>1||v<0||v>1||p<0||p>1000||dist< -1||dist>10000||x< -1||x>=320||y< -1||y>=240||w<0||w>320||h<0||h>240)return;
  if(p>0&&(x<0||y<0||w<1||h<1))return;
  vision.update(seq,a,v,p,dist,x,y,w,h,millis());return;
 }
 if(n==103){lastRemote=millis();return;}
 if(n==100||n==110){standby();reply(true);return;}
 if(n==101){
  if(d<0||d>4){reply(false);return;}
  if(d==4&&!vision.fresh(millis())){reply(false);return;}
  if(d==0)standby();else chooseMode(d,true);reply(true);return;
 }
 if(n==102){
  irOwned=false;
  if(d<1||d>9){reply(false);return;}
  if(d==9){standby();return;}
  if(doc.containsKey("D2")){int sp=doc["D2"];if(sp<0||sp>DRIVE_LIMIT)return;speedSetting=sp;}
  mode=5;manualDir=d;remoteOwned=true;lastRemote=millis();return;
 }
 if(n==104){if(d<0||d>DRIVE_LIMIT)return;speedSetting=d;reply(true);return;}
 if(n==5){int angle=doc["D2"]|90;if(d!=1||angle<0||angle>180){reply(false);return;}standby();setPan(angle);reply(true);return;}
 if(n==106){if(d!=1&&d!=3&&d!=5){reply(false);return;}standby();setPan(d==5?90:panAngle+(d==1?15:-15));reply(true);return;}
 if(n==8){ // same original D1 selector/D2,D3,D4 RGB layout, single LED
  int r=doc["D2"]|0,g=doc["D3"]|0,b=doc["D4"]|0;
  if(r<0||r>255||g<0||g>255||b<0||b>255)return;
  led[0]=CRGB(r,g,b);FastLED.show();reply(true);return;
 }
 if(n==105){if(d==1)FastLED.setBrightness(min(100,(int)FastLED.getBrightness()+5));else if(d==2)FastLED.setBrightness(max(0,(int)FastLED.getBrightness()-5));FastLED.show();return;}
 if(n==201){
  standby();
  if(d!=0&&d!=1){reply(false);return;}
  if(d&&!vision.fresh(millis())){reply(false);return;}
  aiGuard=d;reply(true);return;
 }
 if(n==202){standby();reply(vision.reset(millis()));return;}
 if(n==21||n==22||n==210){telemetry();return;}
 reply(false);
}
void serialInput(){
 while(Serial.available()){
  char c=Serial.read();if(c=='{'){inputPos=0;receiving=true;}
  if(!receiving)continue;
  if(inputPos>=sizeof(input)-1){receiving=false;continue;}
  input[inputPos++]=c;
  if(c=='}'){input[inputPos]=0;receiving=false;command(input);}
 }
}
void sensors(){
 lineL=analogRead(LINE_LEFT_PIN);lineM=analogRead(LINE_MIDDLE_PIN);lineR=analogRead(LINE_RIGHT_PIN);
 batteryMv=(int)(analogRead(BATTERY_PIN)*BATTERY_SCALE*1000);
 static uint32_t lastPing=0;
 if(millis()-lastPing>=100){
  lastPing=millis();digitalWrite(US_TRIG_PIN,LOW);delayMicroseconds(2);
  digitalWrite(US_TRIG_PIN,HIGH);delayMicroseconds(10);digitalWrite(US_TRIG_PIN,LOW);
  unsigned long duration=pulseIn(US_ECHO_PIN,HIGH,15000);rangeCm=duration?(int)(duration/58):-1;
 }
}
bool onBlack(int value){return BLACK_IS_HIGH?value>=LINE_THRESHOLD:value<=LINE_THRESHOLD;}
void floorTrack(int &left,int &right){
 bool l=onBlack(lineL),m=onBlack(lineM),r=onBlack(lineR);
 if(m){left=right=speedSetting;}
 else if(l&&!r){left=0;right=speedSetting;}
 else if(r&&!l){left=speedSetting;right=0;}
 else{left=right=0;reason=4;}
}
void avoid(int &left,int &right){
 uint32_t now=millis();
 if(avoidPhase==0){
  if(rangeCm<0){reason=8;return;}
  if(rangeCm>25){left=right=speedSetting;return;}
  setPan(150);avoidPhase=1;avoidAt=now;return;
 }
 if(avoidPhase==1&&now-avoidAt>=500){rangeLeft=rangeCm;setPan(30);avoidPhase=2;avoidAt=now;return;}
 if(avoidPhase==2&&now-avoidAt>=500){rangeRight=rangeCm;setPan(90);avoidPhase=3;avoidAt=now;return;}
 if(avoidPhase==3&&now-avoidAt>=400){
  if(rangeLeft<0&&rangeRight<0){avoidPhase=0;reason=8;return;}
  avoidPhase=(rangeLeft>=rangeRight)?4:5;avoidAt=now;
 }
 if(avoidPhase==4||avoidPhase==5){
  if(now-avoidAt<350)directionPWM(avoidPhase==4?3:4,speedSetting,left,right);
  else{avoidPhase=0;left=right=0;}
 }
}
void physicalInputs(){
 static bool raw=HIGH,stable=HIGH;static uint32_t keyAt=0;static uint8_t keyMode=0;
 bool k=digitalRead(KEY_PIN);if(k!=raw){raw=k;keyAt=millis();}
 if(millis()-keyAt>40&&stable!=raw){stable=raw;if(stable==LOW){keyMode=(keyMode+1)%4;if(!keyMode)standby();else chooseMode(keyMode,false);}}
 if(remote.decode(&irResult)){
  unsigned long code=irResult.value;remote.resume();
  if(code==0xFFFFFFFF){if(irOwned&&mode==5)lastRemote=millis();return;} // movement expires without a new full command
  if(code==16712445){standby();return;}
  if(code==16738455){chooseMode(1,false);return;}
  if(code==16750695){chooseMode(2,false);return;}
  if(code==16756815){chooseMode(3,false);return;}
  int d=0;if(code==16736925)d=1;if(code==16754775)d=2;if(code==16720605)d=3;if(code==16761405)d=4;
  if(d){irOwned=true;mode=5;manualDir=d;remoteOwned=true;lastRemote=millis();}
 }
}
void setup(){
 Serial.begin(9600);pinMode(MOTOR_STBY,OUTPUT);pinMode(MOTOR_LEFT_PWM,OUTPUT);pinMode(MOTOR_RIGHT_PWM,OUTPUT);
 pinMode(MOTOR_LEFT_DIR,OUTPUT);pinMode(MOTOR_RIGHT_DIR,OUTPUT);motorOutput(0,0);
 pinMode(KEY_PIN,INPUT_PULLUP);pinMode(US_TRIG_PIN,OUTPUT);pinMode(US_ECHO_PIN,INPUT);
 pan.attach(SERVO_PAN_PIN);setPan(90);FastLED.addLeds<NEOPIXEL,RGB_PIN>(led,1);FastLED.setBrightness(20);led[0]=CRGB::Black;FastLED.show();remote.enableIRIn();
}
void loop(){
 serialInput();physicalInputs();sensors();
 if(txPos>=txLen&&pendingReply>=0&&Serial.availableForWrite()>=24){Serial.println(pendingReply?F("{\"N\":211,\"OK\":1}"):F("{\"N\":211,\"OK\":0}"));pendingReply=-1;}
 pumpTelemetry();vision.evaluate(aiGuard,millis());reason=mode?1:0;
 int left=0,right=0;
 if(remoteOwned&&millis()-lastRemote>=(irOwned?300:REMOTE_TIMEOUT_MS)){standby();reason=2;}
 else if(aiGuard&&mode&&!vision.fresh(millis())){standby();reason=3;}
 else if(aiGuard&&vision.latched){reason=5;}
 else if(mode==1||mode==4)floorTrack(left,right);
 else if(mode==2)avoid(left,right);
 else if(mode==3){if(rangeCm>=15&&rangeCm<=50)left=right=speedSetting;else reason=8;}
 else if(mode==5)directionPWM(manualDir,speedSetting,left,right);
 motorOutput(left,right);
 if(millis()-lastTelemetry>=1000){lastTelemetry=millis();telemetry();}
}
