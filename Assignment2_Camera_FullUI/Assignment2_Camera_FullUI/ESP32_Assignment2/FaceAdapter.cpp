// Legacy ESP32 1.0.4 face API, settings retained from the user's Assignment 1.
#include "FaceAdapter.h"
#include "fd_forward.h"
#include "fr_forward.h"
bool faceEnabled=false,faceRecognize=false,faceEnroll=false;
static face_id_list ids={};static mtmn_config_t config={};
void initFaces(){
 config.type=FAST;config.min_face=80;config.pyramid=0.707;config.pyramid_times=4;
 config.p_threshold.score=0.6;config.p_threshold.nms=0.7;config.p_threshold.candidate_number=20;
 config.r_threshold.score=0.7;config.r_threshold.nms=0.7;config.r_threshold.candidate_number=10;
 config.o_threshold.score=0.7;config.o_threshold.nms=0.7;config.o_threshold.candidate_number=1;
 face_id_init(&ids,7,5);
}
FaceOutput processFaces(const uint8_t *pixels,int w,int h){
 FaceOutput out={};out.id=-2;if(!faceEnabled)return out;
 // Face network always gets <=320x240, independently of camera JPEG resolution.
 int fw=w,fh=h;if(fw>320){fh=fh*320/fw;fw=320;}if(fh>240){fw=fw*240/fh;fh=240;}
 auto *m=dl_matrix3du_alloc(1,fw,fh,3);if(!m){out.error=1;return out;}
 for(int y=0;y<fh;y++)for(int x=0;x<fw;x++)memcpy(m->item+3*(y*fw+x),pixels+3*((y*h/fh)*w+x*w/fw),3);
 box_array_t *boxes=face_detect(m,&config);
 if(boxes){out.count=boxes->len<4?boxes->len:4;
  for(int i=0;i<out.count;i++){auto b=boxes->box[i];out.boxes[i]={(int)(b.box_p[0]*w/fw),(int)(b.box_p[1]*h/fh),(int)((b.box_p[2]-b.box_p[0]+1)*w/fw),(int)((b.box_p[3]-b.box_p[1]+1)*h/fh)};}
  if(faceRecognize||faceEnroll){auto *a=dl_matrix3du_alloc(1,FACE_WIDTH,FACE_HEIGHT,3);
   if(!a)out.error=2;else{if(align_face(boxes,m,a)==ESP_OK){if(faceEnroll){out.samples=enroll_face(&ids,a);out.id=ids.tail;if(!out.samples)faceEnroll=false;}else out.id=recognize_face(&ids,a);}else out.error=3;dl_matrix3du_free(a);}
  }
  free(boxes->score);free(boxes->box);free(boxes->landmark);free(boxes);
 }
 dl_matrix3du_free(m);return out;
}
