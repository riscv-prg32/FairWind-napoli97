#ifndef FAIRWIND_FIXMATH_H
#define FAIRWIND_FIXMATH_H
#include <stdint.h>

/* Integer-only trigonometry for the race engine. Angles are 16-bit binary
   angles (65536 = 360 degrees) measured clockwise from world +y, so a
   heading h has the unit vector (sin h, cos h). Results are Q14. */
#define DEG(d) ((int32_t)(d)*182)
#define BAM2DEG(a) ((int32_t)(a)*45/8192)

static const int16_t sin_quarter[65]={0,402,804,1205,1606,2006,2404,2801,3196,3590,3981,4370,4756,5139,5520,5897,6270,6639,7005,7366,7723,8076,8423,8765,9102,9434,9760,10080,10394,10702,11003,11297,11585,11866,12140,12406,12665,12916,13160,13395,13623,13842,14053,14256,14449,14635,14811,14978,15137,15286,15426,15557,15679,15791,15893,15986,16069,16143,16207,16261,16305,16340,16364,16379,16384};
static const int16_t cordic_atan[14]={8192,4836,2555,1297,651,326,163,81,41,20,10,5,3,1};

static int32_t fsin(uint16_t a){
    unsigned q=a>>14,t=a&16383u,i;int32_t v;
    if(q&1u)t=16384u-t;
    i=t>>8;
    v=i>=64u?16384:sin_quarter[i]+(((sin_quarter[i+1]-sin_quarter[i])*(int32_t)(t&255u))>>8);
    return q&2u?-v:v;
}
static int32_t fcos(uint16_t a){return fsin((uint16_t)(a+16384u));}

/* Heading of the vector (dx,dy): CORDIC vectoring, 14 iterations. */
static uint16_t bearing(int32_t dx,int32_t dy){
    int32_t x=dy,y=dx,nx;uint16_t a=0;int i;
    if(!x&&!y)return 0;
    while(x>(1<<24)||x<-(1<<24)||y>(1<<24)||y<-(1<<24)){x>>=1;y>>=1;}
    while(x<(1<<14)&&x>-(1<<14)&&y<(1<<14)&&y>-(1<<14)){x*=2;y*=2;}
    if(x<0){x=-x;y=-y;a=32768u;}
    for(i=0;i<14;i++){
        if(y>0){nx=x+(y>>i);y-=x>>i;a=(uint16_t)(a+cordic_atan[i]);}
        else{nx=x-(y>>i);y+=x>>i;a=(uint16_t)(a-cordic_atan[i]);}
        x=nx;
    }
    return a;
}

static int32_t isqrt(uint32_t n){
    uint32_t r=0,bit=1u<<30;
    while(bit>n)bit>>=2;
    while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}
    return (int32_t)r;
}
#endif
