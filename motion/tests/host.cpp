constexpr int W=96,H=64;
#include "mock_host.hpp"
namespace {
void scene(Buffer& buf,int shift){
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){
        unsigned seed=unsigned((x-shift+W)%W)+unsigned(y)*1009;
        seed=(seed^(seed>>16))*0x45d9f3bU;seed=(seed^(seed>>16))*0x45d9f3bU;
        float lum=float(seed%255)/255.f;
        auto p=buf.v.data()+4*(W*y+x);
        p[0]=lum*1.5f;p[1]=lum;p[2]=lum*0.75f;p[3]=1.f;
    }
}
double render(int a,int b,int c,double shutter=180){
    scene(inputs[0],a);scene(inputs[1],b);scene(inputs[2],c);angle=shutter;
    auto entry=plugin->mainEntry;
    assert(entry(kOfxImageEffectActionGetFramesNeeded,nullptr,nullptr,nullptr)==0);
    assert(requested[0]==(angle>existing||optical?std::max(0.,renderTime-1):renderTime));
    assert(requested[1]==(angle>existing||optical?std::min(2.,renderTime+1):renderTime));
    assert(entry(kOfxImageEffectActionRender,nullptr,nullptr,nullptr)==0);
    assert(fetched==released);
    double error=0;
    for(size_t i=0;i<output.v.size();++i){
        assert(std::isfinite(output.v[i]));
        if(i%4==3)assert(output.v[i]==1);
        error+=std::abs(output.v[i]-inputs[int(renderTime)].v[i]);
    }
    return error/output.v.size();
}
}
int main(int argc,char** argv){
    startHost(argc,argv);
    // A subpixel translation of a smooth textured patch used to yield a zero
    // integer vector. Check displacement in pixels independently of shutter.
    {
        Plane original(W,H), shifted(W,H);
        auto texture=[](float x,float y){return .4f+.14f*std::sin(.31f*x+.1f*y)+.12f*std::cos(.09f*x-.38f*y);};
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            original.a[y*W+x]=texture(x,y);
            shifted.a[y*W+x]=texture(x-.28f,y+.22f);
        }
        auto d=refine(original,shifted,40,32,{},false);
        assert(std::abs(d.x-.28f)<.12f && std::abs(d.y+.22f)<.12f);
    }
    assert(render(3,3,3)==0); // hold
    assert(render(0,4,8,0)==0); // exact bypass
    existing=180;assert(render(0,4,8)==0);existing=0;
    auto forward=render(0,4,8);assert(forward>0.005);auto saved=output.v;
    auto reverse=render(8,4,0);assert(std::abs(forward-reverse)<1e-6);
    for(size_t i=0;i<saved.size();++i)assert(std::abs(saved[i]-output.v[i])<2e-6f);
    assert(render(0,2,8)>0.005); // speed ramp in delivered samples
    assert(render(0,2,4)>0.005); // e.g. 50p -> 25p
    assert(render(2,3,4)>0.001); // conformed 50p at half speed
    for(double t:{0.,2.}){renderTime=t;assert(render(0,4,8)>0.005);}
    renderTime=1;optical=1;
    assert(render(0,6,0,0)>0.02);
    assert(render(2,2,2,0)==0);
    optical=0;
    for(int c:{Linear,SLog3,Intermediate})
        for(float v:{-0.01f,0.f,0.001f,0.18f,1.f,4.f})assert(std::abs(decode(encode(v,c),c)-v)<0.00001f);
    look=25;capture=50;existing=90;angle=180;renderTime=0;
    auto draw=[&](double t){
        renderTime=t;
        assert(plugin->mainEntry(kOfxImageEffectActionGetFramesNeeded,nullptr,nullptr,nullptr)==0);
        assert(plugin->mainEntry(kOfxImageEffectActionRender,nullptr,nullptr,nullptr)==0);
        return output.v;
    };
    // 50p at 100%: Resolve (nearest) presents source 0,2,4 in a 25p timeline.
    // Model a textured translating object with the original 5-ms camera exposure.
    auto signal=[](double x,int y){return .45+.18*std::sin(.46*x+.04*y)+.12*std::cos(.27*x-.19*y)+.07*std::cos(.14*x+.25*y);};
    auto exposed=[&](int x,int y,int nativeFrame,double ms){
        double value=0;
        for(int k=0;k<64;++k)value+=signal(x-4*nativeFrame-0.2*ms*((k+.5)/64.-.5),y);
        return float(value/64.);
    };
    for(int t=0;t<3;++t)for(int y=0;y<H;++y)for(int x=0;x<W;++x){
        float v=exposed(x,y,2*t,5.);
        for(int c=0;c<3;++c)inputs[t].v[4*(y*W+x)+c]=v;
        inputs[t].v[4*(y*W+x)+3]=1.f;
    }
    projectRate=25;hostRate=25;unmapped=50;capturePreset=5;lookPreset=3;existing=90;
    auto result=draw(1);
    double rawError=0,blurError=0;
    int count=0;
    for(int y=12;y<H-12;++y)for(int x=20;x<W-20;++x){
        int i=4*(y*W+x);
        float target=exposed(x,y,2,20.);
        rawError+=std::abs(inputs[1].v[i]-target);blurError+=std::abs(result[i]-target);++count;
    }
    assert(blurError<rawError*.75);
    assert(result!=inputs[1].v);
    changedName="inspectRender";
    assert(plugin->mainEntry(kOfxActionInstanceChanged,nullptr,nullptr,nullptr)==kOfxStatOK);
    assert(reported.find("5.00 → 20.00 ms · vicini 2")!=std::string::npos);
    std::printf("50p 90 deg -> 25p 180 deg: reference MAE %.6f -> %.6f\n",rawError/count,blurError/count);
    // Two foreground edges reveal/cover a stationary textured background.
    // Render each exposure directly from the continuous scene as ground truth.
    auto surface=[](double x,int y,double timeMs){
        double local=x-18.-timeMs*.15;
        if(local>=0 && local<42)return .61+.16*std::sin(local*.51+y*.08);
        return .20+.10*std::cos(x*.32-y*.17);
    };
    auto paint=[&](Buffer& dest,double centerMs,double exposure){
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            double sum=0;
            for(int k=0;k<64;++k)sum+=surface(x,y,centerMs+exposure*((k+.5)/64.-.5));
            auto pixel=dest.v.data()+4*(y*W+x);
            std::fill(pixel,pixel+3,float(sum/64));pixel[3]=1;
        }
    };
    for(int i=0;i<3;++i)paint(inputs[i],40*i,5);
    for(int i=0;i<2;++i)paint(intermediate[i],20+40*i,5);
    Buffer truth;paint(truth,40,20);
    subframeMode=0;auto fallback=draw(1);
    subframeMode=2;assert(draw(1)==fallback);
    subframeMode=1;auto fused=draw(1);
    assert(fused!=fallback);
    reported.clear();changedName="inspectRender";
    assert(plugin->mainEntry(kOfxActionInstanceChanged,nullptr,nullptr,nullptr)==0);
    assert(reported.find("subframe OFX")!=std::string::npos);
    double inputMae=0,outputMae=0,edgeMae=0;int n=0,edges=0;
    for(int y=8;y<H-8;++y)for(int x=6;x<W-6;++x){
        int i=4*(y*W+x);double err=std::abs(fused[i]-truth.v[i]);
        inputMae+=std::abs(inputs[1].v[i]-truth.v[i]);outputMae+=err;++n;
        if(std::abs(x-24)<=2 || std::abs(x-66)<=2){edgeMae+=err;++edges;}
        if(x<12 || x>81)assert(std::abs(fused[i]-inputs[1].v[i])<1e-6);
    }
    std::printf("Subframe 50p/90 -> 25p/180 foreground: source %.6f, fused %.6f, edge %.6f\n",
                inputMae/n,outputMae/n,edgeMae/edges);
    assert(outputMae<inputMae*.65);
    assert(fetched==released);
    for(auto* buf:{&inputs[0],&inputs[1],&inputs[2],&intermediate[0],&intermediate[1]})
        for(size_t k=3;k<buf->v.size();k+=4)buf->v[k]=0;
    auto zeroAlpha=draw(1);
    for(size_t k=0;k<fused.size();++k)assert(zeroAlpha[k]==(k%4==3?0.f:fused[k]));
    diagnostic=1;auto delta=draw(1);
    for(size_t k=0;k<fused.size();++k)
        assert(delta[k]==(k%4==3?1.f:std::min(1.f,8.f*std::abs(fused[k]-inputs[1].v[k]))));
    diagnostic=0;
    for(auto* buf:{&inputs[0],&inputs[1],&inputs[2],&intermediate[0],&intermediate[1]})
        for(size_t k=3;k<buf->v.size();k+=4)buf->v[k]=1;
    subframeMode=0;
    // Same project, host delivering at 50: the automatic look follows 25.
    hostRate=50;auto automaticLook=draw(1);
    lookPreset=1;assert(automaticLook==draw(1));assert(automaticLook==draw(0));
    projectRate=0;capturePreset=lookPreset=0;capture=50;look=25;
    scene(inputs[0],0);scene(inputs[1],4);scene(inputs[2],8);
    auto first=draw(0);assert(first==draw(1));assert(requested[0]==0 && requested[1]==1);
    assert(first!=draw(2));
    hostRate=25;auto converted=draw(1);
    look=0;capture=25;existing=45;assert(converted==draw(1));
    hostRate=100;look=25;capture=100;existing=90;
    first=draw(0);assert(first==draw(1));assert(first==draw(2));
    assert(fetched==released);
    for(double timeline:{24.,25.,50.,100.}){
        hostRate=timeline;
        for(int cp=1;cp<=4;++cp)for(int lp=1;lp<=4;++lp){
            const double rates[]={0,50,50,100,25}, angles[]={0,90,180,180,90};
            const double targets[]={0,25,24,0,25};
            capturePreset=lookPreset=0;capture=rates[cp];existing=angles[cp];
            look=targets[lp];angle=lp==4?90:180;
            auto expected=draw(1);
            capturePreset=cp;lookPreset=lp;look=12;capture=30;angle=0;existing=300;
            assert(expected==draw(1));
        }
    }
    // Keep a stationary textured background with a moving foreground region.
    // This models the input delivered after camera motion has been removed.
    hostRate=25;lookPreset=3;capturePreset=5;existing=0;
    for(int t=0;t<3;++t){
        scene(inputs[t],0);
        Buffer moving;scene(moving,4*t);
        for(int y=8;y<H-8;++y)
            std::copy_n(moving.v.data()+4*(y*W+W/2),4*(W/2),inputs[t].v.data()+4*(y*W+W/2));
    }
    auto stabilized=draw(1);
    double movingDifference=0;
    for(int y=20;y<H-20;++y){
        for(int x=8;x<20;++x)assert(stabilized[4*(y*W+x)]==inputs[1].v[4*(y*W+x)]);
        for(int x=66;x<82;++x)movingDifference+=std::abs(stabilized[4*(y*W+x)]-inputs[1].v[4*(y*W+x)]);
    }
    assert(movingDifference>1.0);
    draftFlag=1;
    auto previewImage=draw(1);
    assert(previewImage!=stabilized);
    preview=1;assert(draw(1)==stabilized);
    draftFlag=0;preview=0;assert(draw(1)==stabilized);

    hostRate=25; capturePreset=5; lookPreset=3; existing=90;
    auto automatic=draw(1);
    capturePreset=1; assert(automatic==draw(1));
    ramp=1; playback=50; auto slow=draw(1);
    ramp=0; capturePreset=2; assert(slow==draw(1));
    ramp=1; playback=0; assert(draw(1)==inputs[1].v);
    ramp=0; capturePreset=lookPreset=0; angle=existing=0;
    hostRate=50;keys=2;
    assert(draw(2)==inputs[2].v);assert(draw(1)==inputs[0].v);assert(draw(2)==inputs[2].v);
    assert(plugin->mainEntry(kOfxActionDestroyInstance,nullptr,nullptr,nullptr)==kOfxStatOK);
    std::printf("Pass: freeze, bypass, exposure, reverse symmetry, mixed cadence, ramp, boundaries, curves. Mean change %.5f\n",forward);
}
