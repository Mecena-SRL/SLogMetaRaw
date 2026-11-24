#include "ofxImageEffect.h"
#include "optical.hpp"
#include "camera.hpp"
#include "exposure.hpp"
#include "blur.hpp"
#include <memory>
#include <mutex>
#include <cstdio>
#include <stdexcept>
extern "C" const char* mecena_source_path_property();
struct SourceRates { double capture, file, exposureMs; };
extern "C" SourceRates mecena_source_rates(const char* path);
#ifdef __APPLE__
#include "blur.hpp"
namespace motion {
bool metalBlur(const blur::Job&,Image,const int*);
}
#endif
using namespace motion;
namespace {
OfxHost* host;
OfxPropertySuiteV1* prop;
OfxImageEffectSuiteV1* fx;
OfxParameterSuiteV1* par;
bool temporal;
constexpr int cinemaLook=5;
void check(OfxStatus status){if(status!=kOfxStatOK)throw status;}
void setHost(OfxHost* h){host=h;}
int integer(OfxPropertySetHandle p,const char* name,int index=0){
    int v;check(prop->propGetInt(p,name,index,&v));return v;
}
double number(OfxPropertySetHandle p,const char* name){
    double v;check(prop->propGetDouble(p,name,0,&v));return v;
}
OfxPropertySetHandle properties(OfxImageEffectHandle e){
    OfxPropertySetHandle p;check(fx->getPropertySet(e,&p));return p;
}
OfxParamHandle parameter(OfxImageEffectHandle e,const char* key){
    OfxParamSetHandle s;check(fx->getParamSet(e,&s));
    OfxParamHandle p;check(par->paramGetHandle(s,key,&p,nullptr));return p;
}
OfxImageClipHandle clip(OfxImageEffectHandle e,const char* name){
    OfxImageClipHandle c;check(fx->clipGetHandle(e,name,&c,nullptr));return c;
}
struct Frame{
    OfxPropertySetHandle p=nullptr;
    Frame(OfxImageClipHandle c,double t){if(fx->clipGetImage(c,t,nullptr,&p)!=kOfxStatOK)p=nullptr;}
    ~Frame(){if(p)fx->clipReleaseImage(p);}
    Frame(const Frame&)=delete;
    Image view()const{
        if(!p)throw OfxStatus(kOfxStatFailed);
        char *depth,*components;
        check(prop->propGetString(p,kOfxImageEffectPropPixelDepth,0,&depth));
        check(prop->propGetString(p,kOfxImageEffectPropComponents,0,&components));
        if(std::strcmp(depth,kOfxBitDepthFloat)||std::strcmp(components,kOfxImageComponentRGBA))throw OfxStatus(kOfxStatErrFormat);
        int b[4];check(prop->propGetIntN(p,kOfxImagePropBounds,4,b));
        void* ptr=nullptr;check(prop->propGetPointer(p,kOfxImagePropData,0,&ptr));
        if(!ptr||b[2]<=b[0]||b[3]<=b[1])throw OfxStatus(kOfxStatFailed);
        return {static_cast<float*>(ptr),b[0],b[1],b[2]-b[0],b[3]-b[1],integer(p,kOfxImagePropRowBytes)};
    }
};
void describe(OfxImageEffectHandle e){
    auto p=properties(e);
    check(prop->propSetString(p,kOfxPropLabel,0,"Mecena Shutter"));
    check(prop->propSetString(p,kOfxImageEffectPluginPropGrouping,0,"Log MetaRaw"));
    check(prop->propSetString(p,kOfxImageEffectPropSupportedContexts,0,kOfxImageEffectContextFilter));
    check(prop->propSetString(p,kOfxImageEffectPropSupportedContexts,1,kOfxImageEffectContextGeneral));
    check(prop->propSetString(p,kOfxImageEffectPropSupportedPixelDepths,0,kOfxBitDepthFloat));
    check(prop->propSetInt(p,kOfxImageEffectPropSupportsTiles,0,0));
    check(prop->propSetInt(p,kOfxImageEffectPropTemporalClipAccess,0,1));
}
void describeContext(OfxImageEffectHandle e){
    OfxPropertySetHandle p;
    for(auto name:{"Source","Output"}){
        check(fx->clipDefine(e,name,&p));
        check(prop->propSetString(p,kOfxImageEffectPropSupportedComponents,0,kOfxImageComponentRGBA));
        if(!std::strcmp(name,"Source"))check(prop->propSetInt(p,kOfxImageEffectPropTemporalClipAccess,0,1));
    }
    OfxParamSetHandle s;check(fx->getParamSet(e,&s));
    auto menu=[&](const char* key,const char* label,std::initializer_list<const char*> choices,int initial){
        check(par->paramDefine(s,kOfxParamTypeChoice,key,&p));
        check(prop->propSetString(p,kOfxPropLabel,0,label));
        int index=0;
        for(auto choice:choices)check(prop->propSetString(p,kOfxParamPropChoiceOption,index++,choice));
        check(prop->propSetInt(p,kOfxParamPropDefault,0,initial));
        check(prop->propSetInt(p,kOfxParamPropAnimates,0,0));
    };
    for(auto key:{"ratesInfo","timelineInfo"}){
        check(par->paramDefine(s,kOfxParamTypeString,key,&p));
        check(prop->propSetString(p,kOfxPropLabel,0,!std::strcmp(key,"ratesInfo")?"Clip · FPS rilevati":"Timeline · FPS rilevati"));
        check(prop->propSetInt(p,kOfxParamPropEnabled,0,0));
        check(prop->propSetInt(p,kOfxParamPropAnimates,0,0));
    }
    check(par->paramDefine(s,kOfxParamTypePushButton,"inspectRender",&p));
    check(prop->propSetString(p,kOfxPropLabel,0,"Verifica ultimo render"));
    check(par->paramDefine(s,kOfxParamTypeString,"renderInfo",&p));
    check(prop->propSetString(p,kOfxPropLabel,0,"Risultato verifica"));
    check(prop->propSetInt(p,kOfxParamPropEnabled,0,0));
    check(prop->propSetInt(p,kOfxParamPropAnimates,0,0));
    check(par->paramDefine(s,kOfxParamTypeString,"samplingInfo",&p));
    check(prop->propSetString(p,kOfxPropLabel,0,"Fotogrammi utilizzati"));
    check(prop->propSetInt(p,kOfxParamPropEnabled,0,0));
    check(prop->propSetInt(p,kOfxParamPropAnimates,0,0));
    menu("diagnosticView","Vista",{"Risultato","Differenza RGB ×8"},0);
    menu("previewQuality","Anteprima",{"Fluida","Alta qualità"},0);
    menu("lookPreset","Look · FPS e shutter",{"Manuale","25 fps · Naturale (180°)","24 fps · Naturale (180°)","Come la timeline · Naturale (180°)","25 fps · Più nitido (90°)","Cinema · shutter sulla timeline"},cinemaLook);
    check(par->paramDefine(s,kOfxParamTypeDouble,"angle",&p));
    check(prop->propSetString(p,kOfxPropLabel,0,"Shutter desiderato (°)"));
    check(prop->propSetDouble(p,kOfxParamPropDefault,0,180));
    check(prop->propSetDouble(p,kOfxParamPropMin,0,0));
    check(prop->propSetDouble(p,kOfxParamPropMax,0,360));
    check(prop->propSetDouble(p,kOfxParamPropDisplayMin,0,0));
    check(prop->propSetDouble(p,kOfxParamPropDisplayMax,0,360));
    check(prop->propSetInt(p,kOfxParamPropAnimates,0,1));
    check(prop->propSetString(p,kOfxParamPropHint,0,"A 25 fps, 180° corrispondono a 20 ms. La posa è centrata sul fotogramma richiesto da Resolve."));

    check(par->paramDefine(s,kOfxParamTypeGroup,"fine",&p));
    check(prop->propSetString(p,kOfxPropLabel,0,"Valori manuali"));
    (void)prop->propSetInt(p,kOfxParamPropGroupOpen,0,0);
    menu("capturePreset","Input · FPS e shutter",{"Manuale","50 fps · 90° (1/200 s)","50 fps · 180° (1/100 s)","100 fps · 180° (1/200 s)","25 fps · 90° (1/100 s)","FPS automatici · shutter manuale"},5);
    check(prop->propSetString(p,kOfxParamPropParent,0,"fine"));
    check(par->paramDefine(s,kOfxParamTypeBoolean,"ramp",&p));
    check(prop->propSetString(p,kOfxParamPropParent,0,"fine"));
    check(prop->propSetString(p,kOfxPropLabel,0,"Compensa lo shutter nella speed ramp"));
    check(prop->propSetString(p,kOfxParamPropHint,0,"Anima Velocità clip seguendo il retime di Resolve. 50% per il rallentatore a metà velocità."));
    check(prop->propSetInt(p,kOfxParamPropDefault,0,0));
    check(par->paramDefine(s,kOfxParamTypeDouble,"playbackPercent",&p));
    check(prop->propSetString(p,kOfxParamPropParent,0,"fine"));
    check(prop->propSetString(p,kOfxPropLabel,0,"Velocità clip (%)"));
    check(prop->propSetDouble(p,kOfxParamPropDefault,0,100));
    check(prop->propSetDouble(p,kOfxParamPropMin,0,-100000));
    check(prop->propSetDouble(p,kOfxParamPropMax,0,100000));
    check(prop->propSetDouble(p,kOfxParamPropDisplayMin,0,-200));
    check(prop->propSetDouble(p,kOfxParamPropDisplayMax,0,400));
    check(par->paramDefine(s,kOfxParamTypeBoolean,"optical",&p));
    check(prop->propSetString(p,kOfxParamPropParent,0,"fine"));
    check(prop->propSetString(p,kOfxPropLabel,0,"Stabilizzazione ottica leggera (senza gyro)"));
    check(prop->propSetInt(p,kOfxParamPropDefault,0,0));
    for(const char* key:{"lookFps","captureFps"}){
        const bool target=std::strcmp(key,"lookFps")==0;
        check(par->paramDefine(s,kOfxParamTypeDouble,key,&p));
        check(prop->propSetString(p,kOfxParamPropParent,0,"fine"));
        check(prop->propSetString(p,kOfxPropLabel,0,target?"FPS look (0 = timeline)":"FPS ripresa (0 = automatici)"));
        check(prop->propSetString(p,kOfxParamPropHint,0,target?
            "Cadenza del movimento. Su una timeline piu veloce ogni posa viene mantenuta.":
            "FPS usati per esprimere lo shutter esistente. A velocita reale usare gli FPS di acquisizione."));
        check(prop->propSetDouble(p,kOfxParamPropDefault,0,0));
        check(prop->propSetDouble(p,kOfxParamPropMin,0,0));
        check(prop->propSetDouble(p,kOfxParamPropMax,0,target?240:2000));
        check(prop->propSetDouble(p,kOfxParamPropDisplayMin,0,0));
        check(prop->propSetDouble(p,kOfxParamPropDisplayMax,0,target?120:240));
        check(prop->propSetInt(p,kOfxParamPropAnimates,0,1));
    }
    check(par->paramDefine(s,kOfxParamTypeDouble,"existing",&p));
    check(prop->propSetString(p,kOfxParamPropParent,0,"fine"));
    check(prop->propSetString(p,kOfxPropLabel,0,"Shutter esistente (gradi)"));
    check(prop->propSetDouble(p,kOfxParamPropDefault,0,0));
    check(prop->propSetDouble(p,kOfxParamPropMin,0,0));
    check(prop->propSetDouble(p,kOfxParamPropMax,0,360));
    check(prop->propSetDouble(p,kOfxParamPropDisplayMin,0,0));
    check(prop->propSetDouble(p,kOfxParamPropDisplayMax,0,360));
    check(par->paramDefine(s,kOfxParamTypeChoice,"curve",&p));
    check(prop->propSetString(p,kOfxParamPropParent,0,"fine"));
    check(prop->propSetString(p,kOfxPropLabel,0,"Segnale in ingresso"));
    int i=0;
    for(auto v:{"Lineare (scene-linear)","Sony S-Log3","DaVinci Intermediate"})
        check(prop->propSetString(p,kOfxParamPropChoiceOption,i++,v));
    check(prop->propSetInt(p,kOfxParamPropDefault,0,0));
    // Hierarchical hosts use Parent. Paged hosts need the actual controls.
    const char* children[]={"lookPreset","angle","timelineInfo","ratesInfo","previewQuality","diagnosticView","inspectRender","samplingInfo","renderInfo",
                            "capturePreset","lookFps","captureFps","existing","ramp","playbackPercent","curve","optical"};
    const auto pageStatus=par->paramDefine(s,kOfxParamTypePage,"Main",&p);
    if(pageStatus==kOfxStatOK){
        int childIndex=0;
        for(auto name:children)
            check(prop->propSetString(p,kOfxParamPropPageChild,childIndex++,name));
    }else if(pageStatus!=kOfxStatErrUnsupported){
        check(pageStatus);
    }
}
void updateControls(OfxImageEffectHandle e){
    if(!par->paramGetPropertySet)return;
    int look=0;
    if(par->paramGetValueAtTime(parameter(e,"lookPreset"),0.,&look)!=kOfxStatOK)return;
    auto enable=[&](const char* name,bool value){
        OfxPropertySetHandle p=nullptr;
        if(par->paramGetPropertySet(parameter(e,name),&p)==kOfxStatOK)
            (void)prop->propSetInt(p,kOfxParamPropEnabled,0,value);
    };
    enable("angle",look==0 || look==cinemaLook);
    for(auto name:{"capturePreset","captureFps","existing"})enable(name,look!=cinemaLook);
    enable("lookFps",look==0);
}
struct DetectedRates {
    double capture, presentation;
    const char* origin;
    double shutterMs=0;
};
bool validRate(double rate){return std::isfinite(rate) && rate>0;}
DetectedRates detect(OfxImageEffectHandle e,OfxPropertySetHandle src,double output){
    char* path=nullptr;
    if(prop->propGetString(properties(e),mecena_source_path_property(),0,&path)==kOfxStatOK && path && *path){
        auto rates=mecena_source_rates(path);
        if(validRate(rates.capture))return {rates.capture,validRate(rates.file)?rates.file:rates.capture,"camera",rates.exposureMs};
        if(validRate(rates.file))return {rates.file,rates.file,"file",rates.exposureMs};
    }
    for(auto key:{kOfxImageEffectPropUnmappedFrameRate,kOfxImageEffectPropFrameRate}){
        double rate=0;
        if(prop->propGetDouble(src,key,0,&rate)==kOfxStatOK && validRate(rate))
            return {rate,rate,"sorgente Resolve"};
    }
    return {output,output,"stima dalla timeline"};
}
struct Clocks {
    double timeline,render;
    bool hasProjectRate;
};
Clocks clocks(OfxImageEffectHandle e,OfxPropertySetHandle output){
    double project=0,render=0;
    bool hasProject=prop->propGetDouble(properties(e),kOfxImageEffectPropFrameRate,0,&project)==kOfxStatOK && validRate(project);
    if(prop->propGetDouble(output,kOfxImageEffectPropFrameRate,0,&render)!=kOfxStatOK || !validRate(render)){
        if(!hasProject)throw OfxStatus(kOfxStatErrValue);
        render=project;
    }
    return {hasProject?project:render,render,hasProject};
}
void reportRates(OfxImageEffectHandle e){
    // Clip properties may not be available yet during CreateInstance.
    try {
        OfxPropertySetHandle src,out;
        check(fx->clipGetPropertySet(clip(e,"Source"),&src));
        check(fx->clipGetPropertySet(clip(e,"Output"),&out));
        auto clock=clocks(e,out);
        auto rates=detect(e,src,clock.timeline);
        char text[180];
        std::snprintf(text,sizeof(text),"%.3f fps (%s) · File %.3f fps",
                      rates.capture,rates.origin,rates.presentation);
        par->paramSetValue(parameter(e,"ratesInfo"),text);
        std::snprintf(text,sizeof(text),"%.3f fps (%s) · Passo OFX %.3f ms",
                      clock.timeline,clock.hasProjectRate?"progetto OFX":"stima da uscita OFX",1000./clock.render);
        par->paramSetValue(parameter(e,"timelineInfo"),text);
    } catch(OfxStatus) {}
}
struct RenderReport {
    std::mutex lock;
    char sampling[160]="In attesa di un render.";
    char message[240]="Nessun render osservato per questa istanza.";
};
RenderReport* reportOf(OfxImageEffectHandle e){
    void* data=nullptr;
    check(prop->propGetPointer(properties(e),kOfxPropInstanceData,0,&data));
    return static_cast<RenderReport*>(data);
}
struct Timing {
    double center, span, existingMs, targetMs;
    double nativeStep=1;
    bool highRateLook=false;
    double stepMs=0;
    bool cinema=false;
};
Timing timing(OfxImageEffectHandle e,double outputTime){
    auto read=[&](const char* name,double t){
        double v;check(par->paramGetValueAtTime(parameter(e,name),t,&v));return v;
    };
    int capturePreset=0,lookPreset=0;
    check(par->paramGetValueAtTime(parameter(e,"lookPreset"),outputTime,&lookPreset));
    constexpr double captureRates[]={0,50,50,100,25,0};
    constexpr double captureAngles[]={0,90,180,180,90,0};
    constexpr double lookRates[]={0,25,24,0,25};
    constexpr double lookAngles[]={0,180,180,180,90};

    OfxPropertySetHandle src,out;
    check(fx->clipGetPropertySet(clip(e,"Source"),&src));
    check(fx->clipGetPropertySet(clip(e,"Output"),&out));
    const auto clock=clocks(e,out);
    const double hostRate=clock.render; // one unit of clipGetImage time
    // Source FPS is only a hint about which upstream images may be available.
    // The duration and centre of the virtual shutter belong to the delivered timeline.
    const auto detected=detect(e,src,clock.timeline);
    const double sourceRate=detected.presentation;
    auto nativeStep=[&](double seconds){
        return std::min(1.,hostRate/sourceRate*std::max(1.,std::ceil(.5*seconds*sourceRate-1.e-9)));
    };
    if(lookPreset==cinemaLook){
        double seconds=std::clamp(read("angle",outputTime),0.,360.)/(360.*clock.timeline);
        double effectiveMs=detected.shutterMs*(detected.capture/detected.presentation);
        int ramp=0;check(par->paramGetValueAtTime(parameter(e,"ramp"),outputTime,&ramp));
        if(ramp){
            double speed=std::abs(read("playbackPercent",outputTime))*.01;
            effectiveMs=speed>1.e-8?effectiveMs/speed:0.;
        }
        return {outputTime,hostRate*seconds,effectiveMs,1000.*seconds,ramp?1.:nativeStep(seconds),false,1000./hostRate,true};
    }
    check(par->paramGetValueAtTime(parameter(e,"capturePreset"),outputTime,&capturePreset));
    if(capturePreset<0||capturePreset>=6||lookPreset<0||lookPreset>=5)throw OfxStatus(kOfxStatErrValue);
    auto targetAt=[&](double t){
        double rate=lookPreset?lookRates[lookPreset]:read("lookFps",t);
        return rate>0?rate:clock.timeline;
    };
    double center=outputTime;
    unsigned keys=0;
    auto rateParam=parameter(e,"lookFps");
    if(!lookPreset && par->paramGetNumKeys)check(par->paramGetNumKeys(rateParam,&keys));
    if(keys || targetAt(outputTime)<hostRate){
        double bounds[2];check(prop->propGetDoubleN(src,kOfxImageEffectPropFrameRange,2,bounds));
        double offset=std::max(0.,outputTime-bounds[0]);
        if(keys){
            // Discrete exposure clock. Reconstruct phase from the clip origin so
            // scrubbing and parallel renders choose the same monotonically ordered pose.
            double first,last;
            check(par->paramGetKeyTime(rateParam,0,&first));
            check(par->paramGetKeyTime(rateParam,keys-1,&last));
            const double frame=std::floor(offset+1e-9);
            double begin=std::clamp(std::floor(first-bounds[0]),0.,frame);
            double end=std::clamp(std::ceil(last-bounds[0]),begin,frame);
            double rate=std::min(1.,targetAt(bounds[0])/hostRate);
            double phase=begin*rate;
            double anchor=std::ceil((std::floor(phase+1e-9)-1e-9)/rate);
            for(double i=begin;i<end;++i){
                double next=phase+std::min(1.,targetAt(bounds[0]+i+0.5)/hostRate);
                if(std::floor(next+1e-9)>std::floor(phase+1e-9))anchor=i+1;
                phase=next;
            }
            rate=std::min(1.,targetAt(bounds[0]+end)/hostRate);
            double cycle=std::floor(phase+(frame-end)*rate+1e-9);
            if(cycle>std::floor(phase+1e-9))anchor=end+std::ceil((cycle-phase-1e-9)/rate);
            center=std::clamp(bounds[0]+anchor,bounds[0],bounds[1]);
        }else{
            double period=hostRate/targetAt(outputTime);
            center=std::clamp(bounds[0]+std::ceil(std::floor(offset/period+1e-9)*period-1e-9),bounds[0],bounds[1]);
        }
    }
    if(center!=outputTime&&!temporal)throw OfxStatus(kOfxStatErrMissingHostFeature);
    double captureRate=capturePreset?captureRates[capturePreset]:read("captureFps",center);
    // Movement vectors are in presentation frames. The sensor shutter in an
    // off-speed file is stretched by capture FPS / file FPS at native playback.
    if(captureRate<=0)captureRate=sourceRate;
    double targetSeconds=(lookPreset?lookAngles[lookPreset]:read("angle",center))/(360.*targetAt(center));
    double existingSeconds=(capturePreset>0 && capturePreset<5?captureAngles[capturePreset]:read("existing",center))/(360.*captureRate);
    int ramp=0;
    check(par->paramGetValueAtTime(parameter(e,"ramp"),center,&ramp));
    if(ramp){
        const double speed=std::abs(read("playbackPercent",center))/100.;
        if(speed<1e-8)return {center,0,0,1000*targetSeconds};
        existingSeconds/=speed;
    }
    // A box shutter of length e convolved with an added box of length a spans a+e:
    // add exactly the missing support so edges get the ramp of the target shutter.
    return {center,clock.render*std::max(0.,targetSeconds-existingSeconds),1000*existingSeconds,1000*targetSeconds,
            ramp?1.:nativeStep(targetSeconds),
            !ramp && std::abs(sourceRate-hostRate)<.001 && sourceRate>targetAt(center)+.001,1000./hostRate,false};
}
void range(OfxImageClipHandle c,double t,double (&r)[2]){
    OfxPropertySetHandle p;check(fx->clipGetPropertySet(c,&p));
    double bounds[2];check(prop->propGetDoubleN(p,kOfxImageEffectPropFrameRange,2,bounds));
    r[0]=t-1>=bounds[0]?t-1:t;r[1]=t+1<=bounds[1]?t+1:t;
}
bool differentPixels(Image a,Image b){
    if(a.w!=b.w || a.h!=b.h || a.x0!=b.x0 || a.y0!=b.y0)return false;
    for(int y=0;y<a.h;++y)
        if(std::memcmp(a.at(a.x0,a.y0+y),b.at(b.x0,b.y0+y),size_t(a.w)*16)!=0)return true;
    return false;
}
bool distinctIntermediate(Image mid,Image center,Image full){
    if(mid.w!=center.w || mid.h!=center.h || mid.x0!=center.x0 || mid.y0!=center.y0)return false;
    // Reject rounded frames, changes in spare alpha, and simple dissolved copies
    // of the endpoints. Distinct upstream samples can still be host-generated.
    double energy=0,projection=0,midEnergy=0;
    int count=0;
    for(int y=0;y<center.h;y+=std::max(1,center.h/54))for(int x=0;x<center.w;x+=std::max(1,center.w/96)){
        auto a=center.at(center.x0+x,center.y0+y), b=full.at(full.x0+x,full.y0+y), c=mid.at(mid.x0+x,mid.y0+y);
        for(int k=0;k<3;++k){
            double ab=b[k]-a[k],ac=c[k]-a[k];
            energy+=ab*ab;projection+=ab*ac;midEnergy+=ac*ac;++count;
        }
    }
    if(!(energy>count*1.e-10))return midEnergy>count*1.e-8;
    double mix=std::clamp(projection/energy,0.,1.);
    double residual=midEnergy-2*mix*projection+mix*mix*energy;
    return residual>std::max(count*1.e-10,energy*.0003);
}
bool opticalEnabled(OfxImageEffectHandle e,double t){
    int active=0;
    check(par->paramGetValueAtTime(parameter(e,"optical"),t,&active));
    return active!=0;
}
std::unique_ptr<OwnedImage> opticalImage(OfxImageClipHandle src,double t,Image im){
    double r[2];range(src,t,r);
    auto output=std::make_unique<OwnedImage>(im);
    auto pyramidNow=pyramid(im,4);
    Vec moves[2]{};
    if(r[0]!=t && r[1]!=t){
        for(int side=0;side<2;++side){
            Frame other(src,r[side]);auto n=other.view();
            if(n.w!=im.w||n.h!=im.h||n.x0!=im.x0||n.y0!=im.y0)throw OfxStatus(kOfxStatErrFormat);
            moves[side]=medianMotion(estimate(pyramidNow,pyramid(n,4)));
        }
    }
    translate(im,output->view,{(moves[0].x+moves[1].x)/3,(moves[0].y+moves[1].y)/3});
    return output;
}
void render(OfxImageEffectHandle e,OfxPropertySetHandle args){
    double outputTime=number(args,kOfxPropTime);
    const Timing plan=timing(e,outputTime);
    double t=plan.center;
    auto src=clip(e,"Source");
    Frame now(src,t),out(clip(e,"Output"),outputTime);
    auto im=now.view(),dest=out.view();
    std::unique_ptr<OwnedImage> savedInput;
    if(im.data==dest.data){
        savedInput=std::make_unique<OwnedImage>(im);
        for(int y=im.y0;y<im.y0+im.h;++y)
            std::memcpy(savedInput->view.at(im.x0,y),im.at(im.x0,y),size_t(im.w)*16);
        im=savedInput->view;
    }
    int win[4];check(prop->propGetIntN(args,kOfxImageEffectPropRenderWindow,4,win));
    int curve;check(par->paramGetValueAtTime(parameter(e,"curve"),t,&curve));
    const bool optics=opticalEnabled(e,t);
    std::unique_ptr<OwnedImage> optCurrent;
    if(optics){
        if(!temporal)throw OfxStatus(kOfxStatErrMissingHostFeature);
        optCurrent=opticalImage(src,t,im);im=optCurrent->view;
    }
    float span=float(plan.span);
    int quality=0,draftRequest=0;
    check(par->paramGetValueAtTime(parameter(e,"previewQuality"),outputTime,&quality));
    const bool draft=quality==0 && prop->propGetInt(args,kOfxImageEffectPropRenderQualityDraft,0,&draftRequest)==kOfxStatOK && draftRequest==1;
    int neighborCount=0;
    bool subframes=false;
    double stepMs=plan.stepMs;
    std::optional<double> observedMs;
    CameraPair camera[2]{};
    bool usedCameraExposure=false;
    int cuts=0;
    double contextTime[2]={0.,0.};
    std::unique_ptr<Frame> wider[2];
    bool enoughExposure=false;
    Image images[2]{};
    std::unique_ptr<Frame> neighbors[2];
    std::unique_ptr<OwnedImage> stabilized[2];
    int view=0;
    check(par->paramGetValueAtTime(parameter(e,"diagnosticView"),outputTime,&view));
    auto record=[&](const char* engine){
        int changed=0,temporalChanges=0,total=0;
        std::vector<float> deltas;
        int x0=std::max({win[0],dest.x0,im.x0}),y0=std::max({win[1],dest.y0,im.y0});
        int x1=std::min({win[2],dest.x0+dest.w,im.x0+im.w}),y1=std::min({win[3],dest.y0+dest.h,im.y0+im.h});
        for(int y=y0;y<y1;y+=std::max(1,(y1-y0)/18))
            for(int x=x0;x<x1;x+=std::max(1,(x1-x0)/32)){
                auto a=im.at(x,y),b=dest.at(x,y);
                float delta=std::max({std::abs(a[0]-b[0]),std::abs(a[1]-b[1]),std::abs(a[2]-b[2])});
                deltas.push_back(std::isfinite(delta)?delta:0.f);
                if(delta>1.e-5f)++changed;
                bool moved=false;
                for(int side=0;side<2;++side)if(neighbors[side]){
                    auto n=images[side].at(x,y);
                    moved|=std::max({std::abs(a[0]-n[0]),std::abs(a[1]-n[1]),std::abs(a[2]-n[2])})>1.e-5f;
                }
                temporalChanges+=moved;
                ++total;
            }
        if(auto* report=reportOf(e)){
            std::lock_guard<std::mutex> guard(report->lock);
            float p90=0.f;
            if(!deltas.empty()){
                auto pos=deltas.begin()+9*(deltas.size()-1)/10;
                std::nth_element(deltas.begin(),pos,deltas.end());
                p90=*pos;
            }
            const char* samples=!neighborCount?"Senza vicini":subframes?"Intermedi OFX":"Interi OFX";
            std::snprintf(report->sampling,sizeof(report->sampling),"%s · passo %.2f ms · %d vicini · frame %.2f · %s",
                          samples,stepMs,neighborCount,outputTime,engine);
            if(contextTime[0]>0 || contextTime[1]>0){
                size_t length=std::strlen(report->sampling);
                std::snprintf(report->sampling+length,sizeof(report->sampling)-length," · guida ±%.2f OFX",std::max(contextTime[0],contextTime[1]));
            }
            if(plan.cinema){
                const char* state=!temporal?"Accesso temporale assente":plan.targetMs==0?"Shutter a zero":
                    !neighborCount?(cuts?"Cambio scena":"Nessun vicino disponibile"):
                    !temporalChanges?"Vicini uguali nel campione":enoughExposure?"Posa già raggiunta":
                    usedCameraExposure?"Posa dai metadati":observedMs?"Posa stimata dai bordi":"Posa di ripresa incerta";
                if(observedMs)
                    std::snprintf(report->message,sizeof(report->message),"%.1f → %.1f ms · %s · ΔRGB P90 %.4f",*observedMs,plan.targetMs,state,p90);
                else
                    std::snprintf(report->message,sizeof(report->message),"%.1f ms · %s · ΔRGB P90 %.4f",plan.targetMs,state,p90);
            }else{
            std::snprintf(report->message,sizeof(report->message),
                "ΔRGB P90 %.4f · RGB %d/%d · %s · frame %.2f · %s · %.2f → %.2f ms · vicini %d · %s",
                p90,changed,total,!neighborCount?"senza vicini":temporalChanges?"ingresso in moto":"vicini uguali nei campioni",
                outputTime,engine,plan.existingMs,plan.targetMs,neighborCount,subframes?"subframe OFX":"frame interi OFX");
            }
        }
        if(view==1){
            for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
                auto a=im.at(x,y),b=dest.at(x,y);
                for(int c=0;c<3;++c)b[c]=std::min(1.f,8.f*std::abs(b[c]-a[c]));
                b[3]=1.f; // Opaque diagnostic view, including on RGB footage with a spare alpha channel.
            }
        }
    };
    if(span>0 && (temporal || !plan.cinema)){
        if(!temporal)throw OfxStatus(kOfxStatErrMissingHostFeature);
        double times[2];range(src,t,times);

        for(int side=0;side<2;++side){
            if(times[side]==t)continue;
            neighbors[side]=std::make_unique<Frame>(src,times[side]);
            if(!neighbors[side]->p && plan.cinema){neighbors[side].reset();continue;}
            images[side]=neighbors[side]->view();
            ++neighborCount;
            auto& n=images[side];
            if(n.w!=im.w||n.h!=im.h||n.x0!=im.x0||n.y0!=im.y0)throw OfxStatus(kOfxStatErrFormat);
            if(optics){stabilized[side]=opticalImage(src,times[side],n);n=stabilized[side]->view;}
        }
        // Request upstream images so stabilization, transforms and color remain in order.
        // Hosts may round fractional requests to either adjacent timeline frame.
        if(!optics && plan.nativeStep>0 && plan.nativeStep<.99999 && neighbors[0] && neighbors[1]){
            std::unique_ptr<Frame> candidates[2];
            Image views[2]{};
            bool distinct=true;
            for(int side=0;side<2;++side){
                candidates[side]=std::make_unique<Frame>(src,t+(side?1:-1)*plan.nativeStep);
                if(!candidates[side]->p){distinct=false;break;}
                try {views[side]=candidates[side]->view();}
                catch(OfxStatus){distinct=false;break;}
                bool usable=plan.cinema?distinctIntermediate(views[side],im,images[side]):
                    differentPixels(views[side],im) && differentPixels(views[side],images[side]);
                if(!usable){distinct=false;break;}
            }
            if(distinct){
                subframes=true;
                for(int side=0;side<2;++side){
                    if(plan.cinema && !draft)wider[side]=std::move(neighbors[side]);
                    neighbors[side]=std::move(candidates[side]);images[side]=views[side];
                }
                span/=float(plan.nativeStep);
                stepMs*=plan.nativeStep;
            }
        }
        // Equal adjacent source exposures tile the requested box shutter under
        // constant motion. The remaining ratios use variance-matched quadrature.
        if(plan.cinema && neighborCount){
            CameraSamples center(im);
            double bounds[2];OfxPropertySetHandle clipProps;
            check(fx->clipGetPropertySet(src,&clipProps));
            check(prop->propGetDoubleN(clipProps,kOfxImageEffectPropFrameRange,2,bounds));
            for(int side=0;side<2;++side){
                if(!neighbors[side])continue;
                if(!differentPixels(im,images[side])){wider[side].reset();continue;}
                CameraSamples near(images[side]);
                auto pair=analysePair(center,near,im.w,im.h);
                if(pair.cut){
                    ++cuts;--neighborCount;neighbors[side].reset();wider[side].reset();
                    continue;
                }
                // Only an ambiguous global fit needs a longer temporal baseline.
                // Revalidate the farther proposal against the immediate pair before
                // accepting it. Far images never enter exposure integration.
                if(!pair.camera.valid() && !draft && !optics){
                    double distance=wider[side]?1.:std::min(3.,std::floor(side?bounds[1]-t:t-bounds[0]));
                    double nearDistance=subframes?plan.nativeStep:1.;
                    if(distance>nearDistance+1.e-5){
                        if(!wider[side])wider[side]=std::make_unique<Frame>(src,t+(side?distance:-distance));
                        if(wider[side]->p){
                            try {
                                Image far=wider[side]->view();
                                if(far.w==im.w && far.h==im.h && far.x0==im.x0 && far.y0==im.y0){
                                    auto longPair=analysePair(center,CameraSamples(far),im.w,im.h);
                                    if(longPair.camera.valid()){
                                        auto seed=shortenedWarp(longPair.camera.forward,nearDistance/distance);
                                        pair=analysePair(center,near,im.w,im.h,&seed);
                                        if(pair.camera.valid())contextTime[side]=distance;
                                    }
                                }
                            } catch(OfxStatus) { /* Optional far context. */ }
                        }
                    }
                }
                wider[side].reset();
                camera[side]=pair.camera;
            }
            // Edge widths are measured AFTER upstream stabilization/retiming. Prefer
            // repeatable sensor metadata when it agrees, or when the image has no
            // identifiable step edges. A confident disagreement uses the rendered image.
            std::optional<double> fraction;
            if(neighborCount==2)fraction=imageExposureFraction(images[0],im,images[1],curve);
            if(fraction)observedMs=*fraction*stepMs;
            if(neighborCount && plan.existingMs>0 &&
               (!observedMs || std::abs(*observedMs-plan.existingMs)<=std::max(1.,.25*plan.existingMs))){
                usedCameraExposure=true;
                observedMs=plan.existingMs;
            }
            const double goal=plan.targetMs/stepMs;
            if(observedMs){
                double existing=*observedMs/stepMs;
                enoughExposure=existing>=goal*.99;
                span=enoughExposure?0.f:float(goal-existing);
            }
        }
    }
    if(span>0 && neighborCount){
        blur::Job job;
        job.center=im;job.prev=neighbors[0]?&images[0]:nullptr;job.next=neighbors[1]?&images[1]:nullptr;
        job.exposure=span;job.curve=curve;job.draft=draft;job.curved=plan.cinema;job.camera=plan.cinema?camera:nullptr;
#ifdef __APPLE__
        if(size_t(im.w)*im.h>=65536 && !fx->abort(e) && metalBlur(job,dest,win)){record("Metal");return;}
#endif
        if(!blur::render(job,dest,win,[&]{return fx->abort(e)!=0;}))return;
        record("CPU");
        return;
    }
    for(int y=std::max(dest.y0,win[1]);y<std::min(dest.y0+dest.h,win[3]);++y){
        for(int x=std::max(dest.x0,win[0]);x<std::min(dest.x0+dest.w,win[2]);++x){
            auto d=dest.at(x,y);int u=x-im.x0,v=y-im.y0;
            if(u<0||v<0||u>=im.w||v>=im.h){std::fill(d,d+4,0.f);continue;}
            std::memcpy(d,im.at(x,y),16);
        }
    }
    record("CPU");
}
OfxStatus entry(const char* action,const void* handle,OfxPropertySetHandle in,OfxPropertySetHandle out){
    auto e=(OfxImageEffectHandle)handle;
    auto is=[&](const char* a){return std::strcmp(action,a)==0;};
    try{
        if(is(kOfxActionLoad)){
            prop=(OfxPropertySuiteV1*)host->fetchSuite(host->host,kOfxPropertySuite,1);
            fx=(OfxImageEffectSuiteV1*)host->fetchSuite(host->host,kOfxImageEffectSuite,1);
            par=(OfxParameterSuiteV1*)host->fetchSuite(host->host,kOfxParameterSuite,1);
            if(!prop||!fx||!par)return kOfxStatErrMissingHostFeature;
            int allow=0;prop->propGetInt(host->host,kOfxImageEffectPropTemporalClipAccess,0,&allow);temporal=allow!=0;
        }else if(is(kOfxActionDescribe))describe(e);
        else if(is(kOfxImageEffectActionDescribeInContext))describeContext(e);
        else if(is(kOfxActionCreateInstance)){
            auto report=std::make_unique<RenderReport>();
            check(prop->propSetPointer(properties(e),kOfxPropInstanceData,0,report.get()));
            report.release();
            updateControls(e);
            reportRates(e);
        }
        else if(is(kOfxActionInstanceChanged)){
            char* name=nullptr;
            if(prop->propGetString(in,kOfxPropName,0,&name)==kOfxStatOK && name && (!std::strcmp(name,"Source") || !std::strcmp(name,"lookPreset") || !std::strcmp(name,"capturePreset")))reportRates(e);
            if(name && !std::strcmp(name,"lookPreset"))updateControls(e);
            if(name && !std::strcmp(name,"inspectRender")){
                reportRates(e);
                if(auto* report=reportOf(e)){
                    char text[sizeof(report->message)],sampling[sizeof(report->sampling)];
                    {std::lock_guard<std::mutex> guard(report->lock);
                     std::memcpy(text,report->message,sizeof(text));std::memcpy(sampling,report->sampling,sizeof(sampling));}
                    check(par->paramSetValue(parameter(e,"samplingInfo"),sampling));
                    check(par->paramSetValue(parameter(e,"renderInfo"),text));
                }
            }
        }else if(is(kOfxActionDestroyInstance)){
            auto* report=reportOf(e);
            check(prop->propSetPointer(properties(e),kOfxPropInstanceData,0,nullptr));
            delete report;
        }
        else if(is(kOfxImageEffectActionGetClipPreferences)){
            auto status=prop->propSetInt(out,kOfxImageEffectFrameVarying,0,1);
            if(status==kOfxStatErrUnknown || status==kOfxStatErrUnsupported)return kOfxStatReplyDefault;
            check(status);
        }
        else if(is(kOfxImageEffectActionGetFramesNeeded)){
            const Timing plan=timing(e,number(in,kOfxPropTime));
            double t=plan.center,r[2]={t,t};
            bool blur=plan.span>0, optical=opticalEnabled(e,t);
            if(temporal&&(blur||optical)){
                auto src=clip(e,"Source");range(src,t,r);
                if(blur && plan.cinema && !optical){
                    OfxPropertySetHandle props;check(fx->clipGetPropertySet(src,&props));
                    double bounds[2];check(prop->propGetDoubleN(props,kOfxImageEffectPropFrameRange,2,bounds));
                    r[0]=std::max(t-3.,bounds[0]);r[1]=std::min(t+3.,bounds[1]);
                }
                if(blur&&optical){
                    double left[2],right[2];range(src,r[0],left);range(src,r[1],right);
                    r[0]=left[0];r[1]=right[1];
                }
            }
            check(prop->propSetDoubleN(out,"OfxImageClipPropFrameRange_Source",2,r));
        }else if(is(kOfxImageEffectActionRender))render(e,in);
        else return kOfxStatReplyDefault;
        return kOfxStatOK;
    }catch(OfxStatus err){
        if(is(kOfxImageEffectActionRender)){
            try {
                if(auto* report=reportOf(e)){
                    double t=number(in,kOfxPropTime);
                    std::lock_guard<std::mutex> guard(report->lock);
                    std::snprintf(report->sampling,sizeof(report->sampling),"Render fallito.");
                    std::snprintf(report->message,sizeof(report->message),"Render frame %.2f fallito (OFX %d).",t,err);
                }
            } catch(OfxStatus) {}
        }
        std::fprintf(stderr,"[Mecena Shutter] %s failed (OFX %d)\n",action,err);
        return err;
    }catch(const std::bad_alloc&){return kOfxStatErrMemory;}catch(...){return kOfxStatFailed;}
}
OfxPlugin plugin={kOfxImageEffectPluginApi,1,"it.mecena.motion.shutter",0,15,setHost,entry};
}
extern "C" OfxPlugin* mecena_shutter_descriptor(){return &plugin;}
