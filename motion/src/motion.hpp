#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>
#include <utility>

namespace motion {
struct Image {
    float *data;
    int x0,y0,w,h,stride; // stride in bytes, can be negative
    float* at(int x,int y) const {
        return reinterpret_cast<float*>(reinterpret_cast<char*>(data)+std::ptrdiff_t(y-y0)*stride)+4*(x-x0);
    }
};
struct Vec { float x=0,y=0; };
enum Curve { Linear, SLog3, Intermediate };
inline float decode(float v,int c) {
    if(c==SLog3) return v>=171.2102946929f/1023 ? 0.19f*std::pow(10.f,(v*1023-420)/261.5f)-0.01f : (v*1023-95)*0.01125f/(171.2102946929f-95);
    if(c==Intermediate) return v<=0.02740668f ? v/10.44426855f : std::exp2(v/0.07329248f-7)-0.0075f;
    return v;
}
inline float encode(float v,int c) {
    if(c==SLog3) return v>=0.01125f ? (420+261.5f*std::log10((v+0.01f)/0.19f))/1023 : (95+v*(171.2102946929f-95)/0.01125f)/1023;
    if(c==Intermediate) return v<=0.00262409f ? v*10.44426855f : (std::log2(v+0.0075f)+7)*0.07329248f;
    return v;
}
struct Plane {
    int w,h; std::vector<float> a;
    Plane(int width,int height):w(width),h(height),a(size_t(w)*h){}
    float get(int x,int y) const {return a[size_t(std::clamp(y,0,h-1))*w+std::clamp(x,0,w-1)];}
};
inline std::vector<Plane> planePyramid(Plane first, unsigned maxLevels) {
    std::vector<Plane> p;
    p.push_back(std::move(first));
    while(p.size()<maxLevels && std::min(p.back().w,p.back().h)>=32){
        auto& a=p.back(); Plane b((a.w+1)/2,(a.h+1)/2);
        for(int y=0;y<b.h;++y) for(int x=0;x<b.w;++x)
            b.a[size_t(y)*b.w+x]=(a.get(2*x,2*y)+a.get(2*x+1,2*y)+a.get(2*x,2*y+1)+a.get(2*x+1,2*y+1))*0.25f;
        p.push_back(std::move(b));
    }
    return p;
}
inline std::vector<Plane> pyramid(Image im, unsigned maxLevels=7) {
    Plane base(im.w,im.h);
    for(int y=0;y<im.h;++y) for(int x=0;x<im.w;++x){
        const auto *s=im.at(im.x0+x,im.y0+y);
        base.a[size_t(y)*im.w+x]=0.25f*s[0]+0.5f*s[1]+0.25f*s[2];
    }
    return planePyramid(std::move(base),maxLevels);
}
struct CameraWarp {
    float h[9]{};
    Vec displacement(int x,int y,int w,int height) const {
        float sx=.5f*std::max(1,w-1), sy=.5f*std::max(1,height-1);
        if(h[8]==0)return {};
        float u=std::clamp(x/sx-1.f,-1.f,1.f),v=std::clamp(y/sy-1.f,-1.f,1.f);
        float z=h[6]*u+h[7]*v+1.f;
        return {sx*((h[0]*u+h[1]*v+h[2])/z-u), sy*((h[3]*u+h[4]*v+h[5])/z-v)};
    }
};
inline Vec track(const std::vector<Plane>& a,const std::vector<Plane>& b,int x,int y,bool nearby=false,const CameraWarp* camera=nullptr){
    int dx=0,dy=0;
    for(int i=int(a.size())-1;i>=0;--i){
        dx*=2; dy*=2;
        int cx=x>>i,cy=y>>i,bx=dx,by=dy;
        float best=std::numeric_limits<float>::max();
        float patch[25];
        for(int j=-2;j<=2;++j)for(int k=-2;k<=2;++k)
            patch[(j+2)*5+k+2]=a[i].get(cx+k,cy+j);
        const Plane& pl=b[i];
        for(int v=dy-2;v<=dy+2;++v) for(int u=dx-2;u<=dx+2;++u){
            float cost=0;
            int ox=cx+u-2,oy=cy+v-2;
            bool interior=ox>=0 && oy>=0 && ox+5<=pl.w && oy+5<=pl.h;
            for(int j=0;j<5 && cost<=best;++j){
                if(interior){
                    const float* row=pl.a.data()+size_t(oy+j)*pl.w+ox;
                    for(int k=0;k<5;++k)cost+=std::abs(patch[j*5+k]-row[k]);
                }else{
                    for(int k=0;k<5;++k)cost+=std::abs(patch[j*5+k]-pl.get(ox+k,oy+j));
                }
            }
            if(cost<best || (cost==best && u*u+v*v<bx*bx+by*by)){
                best=cost;bx=u;by=v;
            }
        }
        // Fine motion on repeated textures can be lost at coarse levels.
        if(nearby && i==0){
            for(int v=-4;v<=4;++v)for(int u=-4;u<=4;++u){
                float cost=0;
                for(int j=0;j<5 && cost<=best;++j)for(int k=0;k<5;++k)
                    cost+=std::abs(patch[j*5+k]-pl.get(cx+u+k-2,cy+v+j-2));
                if(cost<best || (cost==best && u*u+v*v<bx*bx+by*by)){
                    best=cost;bx=u;by=v;
                }
            }
        }
        if(camera && camera->h[8]!=0 && i==0){
            auto predicted=camera->displacement(x,y,pl.w,pl.h);
            int ux=int(std::round(predicted.x)), vy=int(std::round(predicted.y));
            for(int v=vy-2;v<=vy+2;++v)for(int u=ux-2;u<=ux+2;++u){
                float cost=0;
                for(int j=0;j<5 && cost<=best;++j)for(int k=0;k<5;++k)
                    cost+=std::abs(patch[j*5+k]-pl.get(cx+u+k-2,cy+v+j-2));
                if(cost<best || (cost==best && std::hypot(u-predicted.x,v-predicted.y)<std::hypot(bx-predicted.x,by-predicted.y))){
                    best=cost;bx=u;by=v;
                }
            }
        }
        dx=bx;dy=by;
    }
    return {float(dx),float(dy)};
}
struct Field{
    int step;
    int w,h;std::vector<Vec> v;
    Field(int width,int height,int spacing=8):step(spacing),w((width-1)/step+2),h((height-1)/step+2),v(size_t(w)*h){}
    Vec sample(int x,int y)const{
        int i=x/step,j=y/step;float fx=float(x%step)/step,fy=float(y%step)/step;
        Vec r;
        for(int b=0;b<2;++b)for(int a=0;a<2;++a){
            float k=(a?fx:1-fx)*(b?fy:1-fy);auto t=v[size_t(j+b)*w+i+a];r.x+=k*t.x;r.y+=k*t.y;
        }
        return r;
    }
};
// One regularized Lucas-Kanade update around the integer match. A strong
// gradient constrains the normal component even along a straight edge.
inline Vec refine(const Plane& from,const Plane& to,int x,int y,Vec base,bool inconsistent){
    float xx=0,xy=0,yy=0,bx=0,by=0,residual=0,still=0;
    for(int j=-2;j<=2;++j)for(int i=-2;i<=2;++i){
        int u=x+i+int(base.x),v=y+j+int(base.y);
        float ref=from.get(x+i,y+j),r=ref-to.get(u,v);
        float gx=.5f*(to.get(u+1,v)-to.get(u-1,v));
        float gy=.5f*(to.get(u,v+1)-to.get(u,v-1));
        xx+=gx*gx;xy+=gx*gy;yy+=gy*gy;bx+=gx*r;by+=gy*r;
        residual+=std::abs(r);still+=std::abs(ref-to.get(x+i,y+j));
    }
    if(inconsistent && residual>=.25f*still)return {};
    float ridge=1.e-7f+.005f*(xx+yy);
    xx+=ridge;yy+=ridge;
    float det=xx*yy-xy*xy;
    if(det<=1.e-15f)return base;
    return {base.x+std::clamp((yy*bx-xy*by)/det,-.65f,.65f),
            base.y+std::clamp((xx*by-xy*bx)/det,-.65f,.65f)};
}
inline Field estimate(const std::vector<Plane>& cur,const std::vector<Plane>& other,int spacing=8,bool nearby=false,const CameraWarp* forward=nullptr,const CameraWarp* backward=nullptr){
    Field f(cur[0].w,cur[0].h,spacing);
    for(int j=0;j<f.h;++j)for(int i=0;i<f.w;++i){
        int x=std::min(i*f.step,cur[0].w-1),y=std::min(j*f.step,cur[0].h-1);
        Vec d=track(cur,other,x,y,nearby,forward);
        // Suppress inconsistent matches where a surface disappears.
        Vec back=track(other,cur,x+int(d.x),y+int(d.y),nearby,backward);
        d=refine(cur[0],other[0],x,y,d,std::hypot(d.x+back.x,d.y+back.y)>2.f);
        f.v[size_t(j)*f.w+i]=d;
    }
    return f;
}
// Inside a single motion region a smooth vector field is appropriate. Across
// a hand or a guitar neck, test the adjacent motion hypotheses on the actual
// RGB images to avoid averaging opposite motions into a nearly zero vector.
inline Vec guidedSample(const Field& field, Image center, Image neighbor, int x, int y) {
    Vec smooth=field.sample(x,y);
    int cell=(y/field.step)*field.w+x/field.step;
    Vec choices[]={field.v[cell],field.v[cell+1],field.v[cell+field.w],field.v[cell+field.w+1],{}};
    float spread=0;
    for(int j=0;j<4;++j)spread=std::max(spread,std::abs(choices[j].x-smooth.x)+std::abs(choices[j].y-smooth.y));
    if(spread<2.f)return smooth;
    const auto origin=center.at(center.x0+x,center.y0+y);
    auto score=[&](Vec v){
        float sum=0;
        int dx=int(std::round(v.x)),dy=int(std::round(v.y));
        for(int k=0;k<5;++k){
            int px=std::clamp(x+(k==1)-(k==2),0,center.w-1),py=std::clamp(y+(k==3)-(k==4),0,center.h-1);
            auto a=center.at(center.x0+px,center.y0+py);
            auto b=neighbor.at(neighbor.x0+std::clamp(px+dx,0,center.w-1),neighbor.y0+std::clamp(py+dy,0,center.h-1));
            float err=0,edge=0;
            for(int c=0;c<3;++c){err+=std::abs(a[c]-b[c]);edge+=std::abs(a[c]-origin[c]);}
            sum+=err/(1.f+12.f*edge);
        }
        return sum;
    };
    Vec best=smooth;
    float error=score(smooth);
    for(Vec v:choices){
        float alternative=score(v);
        if(alternative+.0005f<error*.94f){best=v;error=alternative;}
    }
    return best;
}
inline Vec exposureOffset(Vec prev,Vec next,float t,bool curved){
    Vec side=t<0?prev:next;
    float f=std::abs(t);
    if(curved && f<=1){
        return {.5f*(t*(next.x-prev.x)+t*t*(prev.x+next.x)),
                .5f*(t*(next.y-prev.y)+t*t*(prev.y+next.y))};
    }
    return {f*side.x,f*side.y};
}
// Sample premultiplied opaque footage in scene-linear light. Keep alpha boundaries sharp.
inline bool linearSample(Image im,float x,float y,int curve,float alpha,float* rgb){
    x=std::clamp(x,0.f,float(im.w-1));y=std::clamp(y,0.f,float(im.h-1));
    int ix=int(x),iy=int(y);
    std::fill(rgb,rgb+3,0.f);
    for(int j=0;j<2;++j)for(int i=0;i<2;++i){
        float w=(i?x-ix:1-x+ix)*(j?y-iy:1-y+iy);
        if(w<=0)continue;
        auto p=im.at(im.x0+std::min(ix+i,im.w-1),im.y0+std::min(iy+j,im.h-1));
        if(!(std::abs(p[3]-alpha)<=1.e-5f))return false;
        for(int c=0;c<3;++c)rgb[c]+=w*decode(p[c],curve);
    }
    return true;
}
inline void reconstruct(Image center,const Image* previous,const Image* next,int x,int y,
                        Vec a,Vec b,float width,int count,int curve,float* out,bool curved=false){
    auto original=center.at(center.x0+x,center.y0+y);
    if(previous && next){
        bool stationary=true;
        for(int dx=-1;dx<=1 && stationary;++dx){
            int px=std::clamp(x+dx,0,center.w-1);
            auto a0=center.at(center.x0+px,center.y0+y);
            stationary=!std::memcmp(a0,previous->at(previous->x0+px,previous->y0+y),16)
                      && !std::memcmp(a0,next->at(next->x0+px,next->y0+y),16);
        }
        if(stationary){std::memcpy(out,original,16);return;}
    }
    float prevLength=a.x*a.x+a.y*a.y,nextLength=b.x*b.x+b.y*b.y;
    bool fit=curved && std::max(prevLength,nextLength)<4.f*std::min(prevLength,nextLength);
    // On a real turnaround both neighbours may lie on the same side of the
    // current position. Verify both correspondences before fitting a parabola.
    if(fit && a.x*b.x+a.y*b.y>=0){
        auto visible=[&](const Image* frame,Vec v){
            float matched[3];
            if(!frame || !linearSample(*frame,x+v.x,y+v.y,curve,original[3],matched))return false;
            for(int k=0;k<3;++k){
                float ref=decode(original[k],curve);
                if(std::abs(ref-matched[k])>.012f+.18f*std::max(std::abs(ref),std::abs(matched[k])))return false;
            }
            return true;
        };
        fit=visible(previous,a) && visible(next,b);
    }
    float sum[3]{};
    for(int i=0;i<count;++i){
        float t=((i+.5f)/count-.5f)*width;
        auto neighbor=t<0?previous:next;
        Vec v=t<0?a:b;
        Vec displacement=exposureOffset(a,b,t,fit);
        float f=std::abs(t),px=x-displacement.x,py=y-displacement.y;
        float cur[3],other[3];
        if(!linearSample(center,px,py,curve,original[3],cur)){std::memcpy(out,original,16);return;}
        if(neighbor && f<=1 && linearSample(*neighbor,px+v.x,py+v.y,curve,original[3],other)){
            float diff=0,peak=.03f;
            for(int c=0;c<3;++c){
                diff=std::max(diff,std::abs(cur[c]-other[c]));
                peak=std::max(peak,std::max(std::abs(cur[c]),std::abs(other[c])));
            }
            // Large disagreement after warping signals a different surface.
            float w=f*std::clamp(1.f-diff/(.02f+.35f*peak),0.f,1.f);
            for(int c=0;c<3;++c)cur[c]+=w*(other[c]-cur[c]);
        }
        for(int c=0;c<3;++c)sum[c]+=cur[c];
    }
    for(int c=0;c<3;++c)out[c]=encode(sum[c]/count,curve);
    out[3]=original[3];
}
inline void integrate(Image im,int x,int y,Vec prev,Vec next,float fraction,int curve,float* out,int maxSamples=64){
    auto center=im.at(x+im.x0,y+im.y0);
    float span=std::max(std::hypot(prev.x,prev.y),std::hypot(next.x,next.y))*fraction;
    if(span<0.01f){std::memcpy(out,center,4*sizeof(float));return;}
    int n=std::clamp(int(std::ceil(span*2)),4,maxSamples);
    float sum[3]={};
    for(int i=0;i<n;++i){
        float t=((i+0.5f)/n-0.5f)*fraction;
        Vec v=t<0?prev:next;
        float px=std::clamp(x-std::abs(t)*v.x,0.f,float(im.w-1));
        float py=std::clamp(y-std::abs(t)*v.y,0.f,float(im.h-1));
        int ix=int(px),iy=int(py);
        for(int b=0;b<2;++b)for(int a=0;a<2;++a){
            auto p=im.at(im.x0+std::min(ix+a,im.w-1),im.y0+std::min(iy+b,im.h-1));
            float w=(a?px-ix:1-px+ix)*(b?py-iy:1-py+iy);
            if(w==0)continue;
            if(!(std::abs(p[3]-center[3])<=1.e-5f)){std::memcpy(out,center,4*sizeof(float));return;}
            for(int k=0;k<3;++k)sum[k]+=w*decode(p[k],curve);
        }
    }
    for(int k=0;k<3;++k)out[k]=encode(sum[k]/n,curve);
    out[3]=center[3];
}
}
