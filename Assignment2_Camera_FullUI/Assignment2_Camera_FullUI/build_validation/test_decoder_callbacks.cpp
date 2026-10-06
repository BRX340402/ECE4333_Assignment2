#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <vector>
static const int ANALYSIS_MAX_W=400,ANALYSIS_MAX_H=400;
struct BoundedJpeg{const uint8_t *src;size_t len;uint8_t *dst;int w,h;};
static size_t readJpeg(void *arg,size_t index,uint8_t *buf,size_t len){auto*d=(BoundedJpeg*)arg;if(index>=d->len)return 0;if(len>d->len-index)len=d->len-index;if(buf)memcpy(buf,d->src+index,len);return len;}
static bool writeJpeg(void *arg,uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t *data){
 auto*d=(BoundedJpeg*)arg;if(!data){if(!x&&!y){if(w>ANALYSIS_MAX_W||h>ANALYSIS_MAX_H||!w||!h)return false;d->w=w;d->h=h;}return true;}
 if(x+w>d->w||y+h>d->h)return false;
 // Preserve the legacy fmt2rgb888 BGR memory order, without a full-size RGB allocation.
 for(int row=0;row<h;row++)for(int col=0;col<w;col++){uint8_t*p=d->dst+3*((y+row)*d->w+x+col);const uint8_t*q=data+3*(row*w+col);p[0]=q[2];p[1]=q[1];p[2]=q[0];}return true;
}

int main(){
 const uint8_t src[4]={1,2,3,4};std::vector<uint8_t> output(400*400*3+2,0xaa);BoundedJpeg d={src,4,output.data()+1,0,0};uint8_t read[8]={};
 assert(readJpeg(&d,2,read,8)==2&&read[0]==3&&read[1]==4);assert(readJpeg(&d,5,read,1)==0);assert(readJpeg(&d,1,nullptr,2)==2);
 assert(!writeJpeg(&d,0,0,401,240,nullptr));assert(writeJpeg(&d,0,0,400,300,nullptr));
 uint8_t block[6]={255,0,0,0,0,255};assert(writeJpeg(&d,398,299,2,1,block));size_t off=3*(299*400+398);assert(d.dst[off]==0&&d.dst[off+2]==255&&d.dst[off+3]==255&&d.dst[off+5]==0);
 assert(!writeJpeg(&d,399,299,2,1,block));assert(output.front()==0xaa&&output.back()==0xaa);
}
