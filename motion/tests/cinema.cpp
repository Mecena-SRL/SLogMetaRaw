// Ground truth is a temporal integration of radiance and foreground visibility.
// No flow estimated by the effect is used to generate a single reference pixel.
constexpr int W=400,H=224;
#include "mock_host.hpp"
#include "camera.hpp"
#include "exposure.hpp"
#include <array>
#include <fstream>
using Color=std::array<float,3>;
namespace {
Color backdrop(float x,float y){
    // Non-repeating, textured room with a few practical lights.
    float s=.5f+.5f*std::sin(.21f*x+.041f*y+1.7f*std::cos(.071f*y));
    float slats=std::pow(.5f+.5f*std::cos(.49f*x-.065f*y),5.f);
    float wood=.5f+.5f*std::cos(.11f*y+.024f*x);
    float lamp=0;
    for(float u:{62.f,173.f,284.f,339.f}){
        float dx=x-u,dy=y-75-.23f*(u-180);
        lamp+=std::max(0.f,1.f-(dx*dx*.048f+dy*dy*.011f));
    }
    return {.065f+.28f*s+.18f*wood+.09f*slats+lamp*1.6f,
            .050f+.19f*s+.09f*wood+.20f*slats+lamp*.9f,
            .045f+.09f*s+.17f*slats+lamp*.38f};
}
struct Segment {float x,y,vx,vy,inverseLength,radius;};
struct Pose {
    float panX,panY,rollCos,rollSin,wristX,wristY,cosHand,sinHand;
    std::array<Segment,6> arm;
    explicit Pose(double ms){
        panX=76.f*std::sin(ms/105.);panY=12.f*std::sin(ms/92.);
        float roll=.039f*std::sin(ms/130.);
        rollCos=std::cos(roll);rollSin=std::sin(roll);
        wristX=185.f-105.f*std::sin(ms/85.);wristY=128.f+12.f*std::sin(ms/80.);
        float twist=-.14f+.30f*std::sin(ms/100.);
        cosHand=std::cos(twist);sinHand=std::sin(twist);
        arm[0]={-8,7,-17,61,1.f/(17*17+61*61),10.f};
        for(int f=0;f<5;++f){
            float u=-15.f+8.f*f, len=f==0?22.f:44.f-4.f*std::abs(f-2);
            float dir=-.39f+.19f*f+.25f*std::sin(ms/99.+.6*f);
            arm[1+f]={u,-4.f,len*std::sin(dir),-len*std::cos(dir),1.f/(len*len),f==0?5.f:3.8f};
        }
    }
    float sample(int x,int y,Color& out) const{
        float px=x-W*.5f-panX,py=y-H*.5f-panY;
        float u=rollCos*px+rollSin*py+W*.5f, v=-rollSin*px+rollCos*py+H*.5f;
        out=backdrop(u,v);
        float dx=u-wristX,dy=v-wristY;
        if(std::abs(dx)>85 || std::abs(dy)>88)return 0;
        float hx=cosHand*dx+sinHand*dy,hy=-sinHand*dx+cosHand*dy;
        float distance=15.f*(1.f-std::sqrt(hx*hx/361.f+hy*hy/256.f));
        for(const auto& seg:arm){
            float sx=hx-seg.x,sy=hy-seg.y;
            float t=std::clamp((sx*seg.vx+sy*seg.vy)*seg.inverseLength,0.f,1.f);
            float qx=sx-t*seg.vx,qy=sy-t*seg.vy;
            distance=std::max(distance,seg.radius-std::sqrt(qx*qx+qy*qy));
        }
        float alpha=std::clamp(distance+.5f,0.f,1.f);
        float texture=.5f+.5f*std::sin(.58f*hx+.21f*hy+.8f*std::sin(.35f*hy));
        Color skin{.66f+.27f*texture,.23f+.23f*texture,.12f+.15f*texture};
        for(int k=0;k<3;++k)out[k]+=alpha*(skin[k]-out[k]);
        return alpha;
    }
};
void expose(Buffer& frame,double time,double ms,int n,std::vector<float>* coverage=nullptr){
    std::fill(frame.v.begin(),frame.v.end(),0.f);
    if(coverage)coverage->assign(W*H,0);
    for(int k=0;k<n;++k){
        Pose pose(time+ms*((k+.5)/n-.5));
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            Color c;float fg=pose.sample(x,y,c);int i=y*W+x;
            if(coverage)(*coverage)[i]+=fg/n;
            for(int ch=0;ch<3;++ch)frame.v[4*i+ch]+=c[ch]/n;
            frame.v[4*i+3]=1;
        }
    }
}
void draw(){
    assert(plugin->mainEntry(kOfxImageEffectActionGetFramesNeeded,nullptr,nullptr,nullptr)==0);
    assert(plugin->mainEntry(kOfxImageEffectActionRender,nullptr,nullptr,nullptr)==0);
    assert(fetched==released);
}
std::string inspect(){
    changedName="inspectRender";reported.clear();
    assert(plugin->mainEntry(kOfxActionInstanceChanged,nullptr,nullptr,nullptr)==0);
    return reported;
}
struct Errors {double raw=0,processed=0;int count=0;};
std::array<Errors,3> evaluate(const Buffer& reference,const std::vector<float>& coverage){
    std::array<Errors,3> e{};
    for(int y=12;y<H-12;++y)for(int x=24;x<W-24;++x){
        int i=y*W+x,region=coverage[i]<.02f?0:coverage[i]>.98f?1:2;
        auto& err=e[region];
        for(int c=0;c<3;++c){
            err.raw+=std::abs(inputs[1].v[i*4+c]-reference.v[i*4+c]);
            err.processed+=std::abs(output.v[i*4+c]-reference.v[i*4+c]);
            ++err.count;
        }
    }
    return e;
}
void prepare(double center,double stepMs,double exposure){
    for(int i=0;i<3;++i)expose(inputs[i],center+(i-1)*stepMs,exposure,20);
    for(int i=0;i<2;++i)expose(intermediate[i],center+(i-.5)*stepMs,exposure,20);
}
void stripeExposure(Buffer& buf,double centerMs,double ms){
    constexpr float bounds[]={52,103,151,208,273,345};
    constexpr float jumps[]={.45f,-.27f,.50f,-.46f,.17f,-.25f};
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){
        float lum=.08f;
        float speed=y<H/2?.8f:-.6f;
        for(int j=0;j<6;++j)
            lum+=jumps[j]*std::clamp(float((x-bounds[j]-speed*centerMs)/(std::abs(speed)*ms)+.5),0.f,1.f);
        auto px=buf.v.data()+4*(y*W+x);
        px[0]=lum;px[1]=.8f*lum;px[2]=.55f*lum;px[3]=1;
    }
}
enum class TimingEvent { Start, Stop, Reverse };
void renderTimingCard(Buffer& frame,TimingEvent event,double centerMs,double shutter,int steps){
    constexpr int repeat=28;
    for(int y=0;y<repeat;++y)for(int x=0;x<W;++x){
        double light=0;
        for(int n=0;n<steps;++n){
            double t=centerMs+shutter*((n+.5)/steps-.5);
            double distance=event==TimingEvent::Start?.52*std::max(0.,t):
                            event==TimingEvent::Stop?.52*std::min(0.,t):.011*t*t;
            double q=x-(x>=80?distance:0.);
            light+=.43+.16*std::sin(.36*q+.1*y+.35*std::sin(.063*q))
                      +.1*std::cos(.082*q-.15*y);
        }
        auto pixel=frame.v.data()+4*(y*W+x);
        pixel[0]=float(light/steps);pixel[1]=.82f*pixel[0];pixel[2]=.6f*pixel[0];pixel[3]=1;
    }
    for(int y=repeat;y<H;++y)
        std::copy_n(frame.v.data()+4*W*(y%repeat),4*W,frame.v.data()+4*W*y);
}
Image imageOf(Buffer& buf){return {buf.v.data(),0,0,W,H,W*16};}

}
int main(int argc,char** argv){
    startHost(argc,argv);
    projectRate=hostRate=25;unmapped=50;lookPreset=5;angle=180;renderTime=1.;
    prepare(0.,40.,5.);
    Buffer ref;std::vector<float> coverage;expose(ref,0.,20.,96,&coverage);
    subframeMode=0;draw();auto oddOnly=output.v;
    std::printf("Timeline only: %s\n",inspect().c_str());
    assert(reported.find("Interi OFX · passo 40.00 ms")!=std::string::npos);
    auto score=evaluate(ref,coverage);
    for(int i=0;i<3;++i){
        auto e=score[i];std::printf("Mixed 50/90 -> 25/180, region %d: %.5f -> %.5f (%d RGB samples)\n",i,e.raw/e.count,e.processed/e.count,e.count);
        const double limit[]={.15,.30,.65};
        assert(e.count>250 && e.processed<limit[i]*e.raw);
    }
    // Changing every manual source field must not change a single output pixel.
    capturePreset=4;capture=13;existing=360;look=7;ramp=1;playback=0;
    draw();assert(output.v==oddOnly);
    ramp=0;
    subframeMode=2;draw();assert(output.v==oddOnly);
    subframeMode=1;draw();assert(output.v!=oddOnly);
    assert(inspect().find("Intermedi OFX · passo 20.00 ms")!=std::string::npos);
    for(auto e:evaluate(ref,coverage))assert(e.processed<e.raw);
    // Cross-dissolved temporal requests must not be mistaken for extra observations.
    for(int i=0;i<2;++i)for(size_t k=0;k<output.v.size();++k)intermediate[i].v[k]=.5f*(inputs[i].v[k]+inputs[i+1].v[k]);
    draw();assert(output.v==oddOnly);
    // Native output of 50 Hz upstream of a 25 Hz project. A request at 1 stays
    // centred on frame 1, using half the sampling interval for the same 20 ms shutter.
    subframeMode=0;hostRate=50;prepare(0.,20.,5.);draw();
    assert(inspect().find("passo 20.00 ms")!=std::string::npos);
    for(auto e:evaluate(ref,coverage))assert(e.processed<e.raw);
    // Odd-only sampling of a near turnaround versus a trajectory that continues
    // linearly. With visible neighbours on the same side, speed at the centre is zero.
    auto apex=exposureOffset({16,0},{16,0},.25f,true);
    assert(std::abs(apex.x-1.f)<1.e-6);
    auto steady=exposureOffset({-16,0},{16,0},.25f,true);
    assert(std::abs(steady.x-4.f)<1.e-6);
    // Same material with irregular acquisition and timeline rates.
    projectRate=hostRate=24000./1001.;unmapped=100;angle=135;
    // Nearest source sampling at 100 Hz on this 23.976 Hz timeline.
    for(int i=0;i<3;++i)expose(inputs[i],1000./unmapped*std::round((i-1)*unmapped/hostRate),2.5,20);
    draw();
    expose(ref,0.,1000.*135./(360.*projectRate),96,&coverage);
    for(auto e:evaluate(ref,coverage))assert(e.processed<e.raw);
    angle=0;draw();assert(output.v==inputs[1].v);
    angle=360;inputs[0]=inputs[1];inputs[2]=inputs[1];draw();assert(output.v==inputs[1].v);
    projectRate=hostRate=25;unmapped=50;angle=180;ramp=0;subframeMode=0;
    for(double shutter:{5.,20.,32.}){
        for(int i=0;i<3;++i)stripeExposure(inputs[i],(i-1)*40.,shutter);
        auto observed=imageExposureFraction(imageOf(inputs[0]),imageOf(inputs[1]),imageOf(inputs[2]),Linear);
        assert(observed && std::abs(*observed*40.-shutter)<1.5);
        draw();
        if(shutter>=20.)assert(output.v==inputs[1].v);
        else assert(output.v!=inputs[1].v);
    }
    // At a clip cut, temporal colours on the other side must not bleed in.
    auto flat=[](Buffer& buf,float value){
        for(size_t i=0;i<buf.v.size();i+=4){std::fill_n(buf.v.data()+i,3,value);buf.v[i+3]=1;}
    };
    flat(inputs[0],.95);flat(inputs[1],.01);flat(inputs[2],.95);draw();
    assert(output.v==inputs[1].v && inspect().find("Cambio scena")!=std::string::npos);
    // The nearest three pictures are unchanged when distant unrelated images appear.
    // Insufficient background features provoke an adaptive, bounded context request.
    for(int i=0;i<3;++i)stripeExposure(inputs[i],(i-1)*40.,5.);
    draw();auto boundary=output.v;
    extendedTimeline=true;
    for(auto& outer:outerFrames)flat(outer,.98f);
    draw();assert(farReads>0 && requested[0]==-2. && requested[1]==4.);
    assert(output.v==boundary);
    extendedTimeline=false;
    // Motion can begin, end, or turn within one exposure on a repeating pattern.
    // A stationary section of the same image must stay sharp in all three cases.
    for(auto event:{TimingEvent::Start,TimingEvent::Stop,TimingEvent::Reverse}){
        for(int i=0;i<3;++i)renderTimingCard(inputs[i],event,(i-1)*40.,5.,12);
        renderTimingCard(ref,event,0.,20.,48);
        draw();
        double raw=0,processed=0;
        int count=0;
        for(int y=10;y<H-10;++y)for(int x=10;x<W-25;++x){
            int k=4*(y*W+x);
            if(x<60)assert(output.v[k]==inputs[1].v[k]);
            if(x<115)continue;
            raw+=std::abs(inputs[1].v[k]-ref.v[k]);
            processed+=std::abs(output.v[k]-ref.v[k]);
            ++count;
        }
        const char* label=event==TimingEvent::Start?"start":event==TimingEvent::Stop?"stop":"turnaround";
        std::printf("Temporal %s: %.6f -> %.6f\n",label,raw/count,processed/count);
        assert(processed<.85*raw);
    }
    // The same odd-numbered frame can repeat while a distinct intermediate moves.
    inputs[0]=inputs[2]=inputs[1];subframeMode=1;
    stripeExposure(intermediate[0],-12.,5.);stripeExposure(intermediate[1],12.,5.);
    draw();assert(inspect().find("Intermedi OFX")!=std::string::npos && output.v!=inputs[1].v);
#ifndef TEST_PACKAGED_OFX
    // No tag reader is installed in the standalone host. Inject a known 5 ms
    // camera measurement at its C ABI and exercise the real timing/render path.
    taggedExposure=true;subframeMode=0;prepare(0.,40.,5.);draw();
    assert(inspect().find("5.0 → 20.0 ms · Posa dai metadati")!=std::string::npos);
    ramp=1;playback=50;draw();
    assert(inspect().find("10.0 → 20.0 ms · Posa dai metadati")!=std::string::npos);
#endif
    assert(plugin->mainEntry(kOfxActionDestroyInstance,nullptr,nullptr,nullptr)==0);
}
