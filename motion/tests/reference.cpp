// Same translating scene rendered once with an instantaneous exposure and once
// with 32 temporal samples over a 180-degree shutter. Evaluate the actual kernel.
#include "motion.hpp"
#include <cstdio>
#include <fstream>
using namespace motion;
constexpr int W=320,H=128;
float scene(float x,float y){
    return 0.44f+0.19f*std::sin(0.42f*x+0.065f*y)+0.14f*std::cos(0.22f*x-0.26f*y)+0.10f*std::cos(0.075f*x+0.32f*y);
}
struct TestFrame {
    std::vector<float> buf=std::vector<float>(W*H*4,1);
    Image img{buf.data(),0,0,W,H,W*16};
    TestFrame(int offset){
        for(int y=0;y<H;++y)for(int x=0;x<W;++x)for(int c=0;c<3;++c)
            img.at(x,y)[c]=scene(x-offset,y);
    }
};
int main(){
    TestFrame prev(-8),now(0),next(8);
    auto p=pyramid(now.img);
    auto a=estimate(p,pyramid(prev.img)), b=estimate(p,pyramid(next.img));
    double baseline=0,processed=0;int count=0;
    for(int y=12;y<H-12;++y)for(int x=16;x<W-16;++x){
        double ref=0;
        for(int s=0;s<32;++s)ref+=scene(x-4*((s+0.5f)/32-0.5f),y)/32;
        float out[4];integrate(now.img,x,y,a.sample(x,y),b.sample(x,y),0.5f,Linear,out);
        baseline+=std::abs(ref-now.img.at(x,y)[0]);processed+=std::abs(ref-out[0]);++count;
    }
    std::printf("Synthetic reference MAE (scene-linear): bypass %.6f, processed %.6f\n",baseline/count,processed/count);
    return processed<baseline?0:1;
}
