// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "motion.hpp"
namespace motion {
inline Vec medianMotion(const Field& f){
    std::vector<float> xs,ys;
    for(int j=1;j<f.h-1;++j)for(int i=1;i<f.w-1;++i){
        const auto v=f.v[size_t(j)*f.w+i];
        xs.push_back(v.x);ys.push_back(v.y);
    }
    if(xs.empty())return {};
    const auto n=xs.size()/2;
    std::nth_element(xs.begin(),xs.begin()+n,xs.end());
    std::nth_element(ys.begin(),ys.begin()+n,ys.end());
    return {xs[n],ys[n]};
}
struct OwnedImage {
    std::vector<float> data;
    Image view;
    explicit OwnedImage(Image im):data(size_t(im.w)*im.h*4),view{data.data(),im.x0,im.y0,im.w,im.h,im.w*16}{}
    OwnedImage(const OwnedImage&)=delete;
};
inline void translate(Image src,Image dst,Vec offset){
    for(int j=0;j<src.h;++j)for(int i=0;i<src.w;++i){
        const float x=std::clamp(i-offset.x,0.f,float(src.w-1));
        const float y=std::clamp(j-offset.y,0.f,float(src.h-1));
        const int ix=int(x),iy=int(y);
        auto out=dst.at(src.x0+i,src.y0+j);
        const auto l=src.at(src.x0+ix,src.y0+iy);
        const auto r=src.at(src.x0+std::min(ix+1,src.w-1),src.y0+iy);
        const auto u=src.at(src.x0+ix,src.y0+std::min(iy+1,src.h-1));
        const auto ur=src.at(src.x0+std::min(ix+1,src.w-1),src.y0+std::min(iy+1,src.h-1));
        const float fx=x-ix,fy=y-iy;
        for(int k=0;k<4;++k){
            float a=l[k]+fx*(r[k]-l[k]),b=u[k]+fx*(ur[k]-u[k]);
            out[k]=a+fy*(b-a);
        }
    }
}
}
