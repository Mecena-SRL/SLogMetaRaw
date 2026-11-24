// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "motion.hpp"
#include <array>
#include <cstdint>

namespace motion {
// Normalized image coordinates are [-1,1] in both dimensions. A disabled
// homography is all zero; this POD is also the argument layout of the Metal flow pass.
struct CameraPair {
    CameraWarp forward{}, reverse{};
    bool valid() const { return forward.h[8] != 0; }
};
static_assert(sizeof(CameraPair)==18*sizeof(float));
struct CameraObservation {
    double x,y,u,v;
};
struct CameraSamples {
    std::vector<Plane> pyramid;
    std::array<int,32> histogram{};
    int stride;
    explicit CameraSamples(Image im) {
        stride = std::max(1,(std::max(im.w,im.h)+383)/384);
        int w=(im.w+stride-1)/stride, h=(im.h+stride-1)/stride;
        Plane base(w,h);
        int a=(stride-1)/4, b=stride-1-a;
        for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
            float sum=0;
            for (int j:{a,b}) for (int i:{a,b}) {
                auto p=im.at(im.x0+std::min(im.w-1,x*stride+i),im.y0+std::min(im.h-1,y*stride+j));
                sum+=.25f*p[0]+.5f*p[1]+.25f*p[2];
            }
            float value=std::isfinite(sum)?sum*.25f:0.f;
            base.a[size_t(y)*w+x]=value;
            ++histogram[static_cast<int>(std::clamp(value*32.f,0.f,31.f))];
        }
        pyramid=planePyramid(std::move(base),6);
    }
};

inline bool fitWarp(const std::vector<CameraObservation>& matches, const std::vector<int>& ids, CameraWarp& warp) {
    // Least squares in normalized coordinates. Pivoting rejects samples on a line.
    double a[8][9]{};
    for (int i:ids) {
        const auto& m=matches[i];
        double rows[2][9]={{m.x,m.y,1,0,0,0,-m.x*m.u,-m.y*m.u,m.u},
                           {0,0,0,m.x,m.y,1,-m.x*m.v,-m.y*m.v,m.v}};
        for(const auto& row:rows) for(int j=0;j<8;++j) for(int k=j;k<9;++k) a[j][k]+=row[j]*row[k];
    }
    for(int j=0;j<8;++j)for(int k=0;k<j;++k)a[j][k]=a[k][j];
    for(int col=0;col<8;++col){
        int pivot=col;
        for(int r=col+1;r<8;++r)if(std::abs(a[r][col])>std::abs(a[pivot][col]))pivot=r;
        if(std::abs(a[pivot][col])<1e-8)return false;
        for(int k=col;k<=8;++k)std::swap(a[col][k],a[pivot][k]);
        double divisor=a[col][col];
        for(int k=col;k<=8;++k)a[col][k]/=divisor;
        for(int r=0;r<8;++r)if(r!=col){
            double s=a[r][col];for(int k=col;k<=8;++k)a[r][k]-=s*a[col][k];
        }
    }
    for(int i=0;i<8;++i){
        if(!std::isfinite(a[i][8]) || std::abs(a[i][8])>8)return false;
        warp.h[i]=float(a[i][8]);
    }
    warp.h[8]=1;
    // Reject folds, reflections and horizons passing through the image.
    return std::abs(warp.h[6])+std::abs(warp.h[7])<.75f && warp.h[0]*warp.h[4]-warp.h[1]*warp.h[3]>.15f;
}
struct PairAnalysis { CameraPair camera; bool cut=false; };

// Estimate the dominant translation over many separated image patches first.
// A small repeated texture (wall tiles, guitar strings) can match the wrong
// local minimum everywhere when every tracker starts from zero independently.
inline double alignmentCost(const Plane& a,const Plane& b,int dx,int dy) {
    // Several moving layers can occupy the frame. Score the dominant fraction
    // of the matches while retaining a smaller contribution from every patch.
    double cost=0, groupSum[20]{};
    int samples=0, groupSize[20]{};
    for(int y=2;y<a.h-2;y+=std::max(1,a.h/13))for(int x=2;x<a.w-2;x+=std::max(1,a.w/19)){
        int u=x+dx,v=y+dy;
        if(u<1||v<1||u>=a.w-1||v>=a.h-1)continue;
        float ref=a.get(x,y),other=b.get(u,v);
        float diff=std::abs(ref-other)/(.12f+.5f*(std::abs(ref)+std::abs(other)));
        const float limited=std::min(.30f,diff);
        int bin=std::min(19,int(limited*64.f));
        cost+=limited;groupSum[bin]+=limited;++groupSize[bin];++samples;
    }
    // Bounds vary per candidate. Penalise the portion of the frame that has
    // no overlap so a distant repetition must explain the scene better.
    double lost=1.-(1.-std::abs(dx)/double(a.w))*(1.-std::abs(dy)/double(a.h));
    if(samples<12)return 1.;
    int kept=3*samples/4,remaining=kept;double robust=0.;
    for(int k=0;k<20 && remaining;++k){
        int take=std::min(remaining,groupSize[k]);
        if(take)robust+=groupSum[k]*take/groupSize[k];
        remaining-=take;
    }
    return .85*robust/kept+.15*cost/samples+.042*lost;
}
inline Vec translationHint(const CameraSamples& from,const CameraSamples& to) {
    int top=0;
    while(top+1<int(from.pyramid.size()) && from.pyramid[top].w>80 && from.pyramid[top].h>40)++top;
    struct Candidate {int x,y;double cost;};
    std::vector<Candidate> seeds{{0,0,0}};
    for(int level=top;level>=0;--level){
        const auto& a=from.pyramid[level];const auto& b=to.pyramid[level];
        int rx=level==top?std::min(32,a.w/3):3,ry=level==top?std::min(24,a.h/3):3;
        std::vector<Candidate> candidates;
        for(auto seed:seeds)for(int v=2*seed.y-ry;v<=2*seed.y+ry;++v)for(int u=2*seed.x-rx;u<=2*seed.x+rx;++u)
            candidates.push_back({u,v,alignmentCost(a,b,u,v)});
        // Fine stripes and edges can disappear at coarse scales. Seed the
        // next scale independently around zero before discarding hypotheses.
        if(level<top){
            int radius=level==0?12:8;
            for(int v=-radius;v<=radius;++v)for(int u=-radius;u<=radius;++u)
                candidates.push_back({u,v,alignmentCost(a,b,u,v)});
        }
        std::sort(candidates.begin(),candidates.end(),[](Candidate a,Candidate b){
            if(a.cost==b.cost)return a.x*a.x+a.y*a.y<b.x*b.x+b.y*b.y;
            return a.cost<b.cost;
        });
        seeds.clear();
        for(auto c:candidates){
            bool close=std::any_of(seeds.begin(),seeds.end(),[&](Candidate s){return std::abs(c.x-s.x)<=2 && std::abs(c.y-s.y)<=2;});
            if(!close)seeds.push_back(c);
            if(seeds.size()==6 || level==0)break;
        }
    }
    return {float(seeds.front().x),float(seeds.front().y)};
}
inline Vec matchCameraFeature(const Plane& a,const Plane& b,int x,int y,Vec hint) {
    float best=std::numeric_limits<float>::infinity();Vec found{float(b.w),float(b.h)};
    // A projective pan can have a different speed at the edge of a wide shot.
    constexpr int radius=5;
    int ox=int(std::round(hint.x)),oy=int(std::round(hint.y));
    for(int v=oy-radius;v<=oy+radius;++v)for(int u=ox-radius;u<=ox+radius;++u){
        if(x+u<3||y+v<3||x+u+3>=b.w||y+v+3>=b.h)continue;
        float cost=.00005f*(std::abs(u-ox)+std::abs(v-oy));
        for(int sy=-3;sy<=3 && cost<best;++sy)for(int sx=-3;sx<=3;++sx)
            cost+=std::abs(a.get(x+sx,y+sy)-b.get(x+u+sx,y+v+sy));
        if(cost<best){best=cost;found={float(u),float(v)};}
    }
    return found;
}
inline CameraWarp shortenedWarp(CameraWarp warp, double fraction){
    for(int i=0;i<8;++i){
        float identity=(i==0||i==4)?1.f:0.f;
        warp.h[i]=identity+float(fraction)*(warp.h[i]-identity);
    }
    return warp;
}
inline PairAnalysis analysePair(const CameraSamples& from,const CameraSamples& to,int width,int height,
                                const CameraWarp* proposal=nullptr) {
    const Plane& a=from.pyramid.front(), &b=to.pyramid.front();
    std::vector<CameraObservation> matches;
    const double sx=2./std::max(1,width-1),sy=2./std::max(1,height-1);
    Vec hint=proposal?Vec{}:translationHint(from,to);
    int nx=std::min(14,a.w/8),ny=std::min(10,a.h/8);
    for(int j=0;j<ny;++j)for(int i=0;i<nx;++i) {
        int x=(2*i+1)*a.w/(2*nx), y=(2*j+1)*a.h/(2*ny);
        // Choose a textured location within each cell; straight featureless
        // edges alone cannot constrain a projective transform.
        int px=x,py=y;float best=1.e-10f;
        for(int oy:{-2,0,2})for(int ox:{-2,0,2}){
            int u=x+ox,v=y+oy;float xx=0,xy=0,yy=0;
            for(int cy:{-1,1})for(int cx:{-1,1}){
                float gx=a.get(u+cx+1,v+cy)-a.get(u+cx-1,v+cy);
                float gy=a.get(u+cx,v+cy+1)-a.get(u+cx,v+cy-1);
                xx+=gx*gx;xy+=gx*gy;yy+=gy*gy;
            }
            float corner=(xx*yy-xy*xy)/(xx+yy+.000001f);
            if(corner>best){best=corner;px=u;py=v;}
        }
        if(best<=1.e-10f)continue;
        Vec localHint=hint;
        if(proposal){
            auto v=proposal->displacement(px*from.stride,py*from.stride,width,height);
            localHint={v.x/from.stride,v.y/from.stride};
        }
        Vec d=matchCameraFeature(a,b,px,py,localHint);
        int qx=px+int(d.x),qy=py+int(d.y);
        if(qx<2||qy<2||qx+2>=a.w||qy+2>=a.h)continue;
        Vec back=matchCameraFeature(b,a,qx,qy,{-localHint.x,-localHint.y});
        if(std::hypot(d.x+back.x,d.y+back.y)>1.5f)continue;
        d=refine(a,b,px,py,d,false);
        double x0=px*from.stride+(from.stride-1)*.5,y0=py*from.stride+(from.stride-1)*.5;
        matches.push_back({sx*x0-1,sy*y0-1,sx*(x0+d.x*from.stride)-1,sy*(y0+d.y*from.stride)-1});
    }
    PairAnalysis result{};
    double overlap=0;
    for(size_t k=0;k<from.histogram.size();++k)overlap+=std::min(from.histogram[k],to.histogram[k]);
    // A large histogram discontinuity alone can be a whip pan or lighting change.
    result.cut=matches.size()<6 && overlap<.18*a.w*a.h;
    if(matches.size()<12)return result;
    std::vector<int> bestIds,ids,seed(4);
    std::uint32_t state=0x8cab22c5u;
    for(int iter=0;iter<96;++iter){
        for(auto& id:seed){state=1664525u*state+1013904223u;id=int((std::uint64_t(state)*matches.size())>>32);}
        CameraWarp guess{};
        if(!fitWarp(matches,seed,guess))continue;
        ids.clear();
        for(size_t i=0;i<matches.size();++i){
            auto& p=matches[i];
            double z=guess.h[6]*p.x+guess.h[7]*p.y+1;
            double ex=((guess.h[0]*p.x+guess.h[1]*p.y+guess.h[2])/z-p.u)/(sx*from.stride);
            double ey=((guess.h[3]*p.x+guess.h[4]*p.y+guess.h[5])/z-p.v)/(sy*from.stride);
            if(ex*ex+ey*ey<=2.25)ids.push_back(int(i));
        }
        if(ids.size()>bestIds.size())bestIds=ids;
        if(bestIds.size()==matches.size())break;
    }
    if(bestIds.size()<12 || bestIds.size()*2<matches.size())return result;
    bool quadrants[4]{};
    for(auto id:bestIds){const auto& m=matches[id];quadrants[(m.x>0)+2*(m.y>0)]=true;}
    if(std::count(quadrants,quadrants+4,true)<3)return result;
    CameraPair pair{};
    if(!fitWarp(matches,bestIds,pair.forward))return result;
    for(auto& m:matches){std::swap(m.x,m.u);std::swap(m.y,m.v);}
    if(fitWarp(matches,bestIds,pair.reverse))result.camera=pair;
    return result;
}
}
