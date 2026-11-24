#pragma once
// Minimal temporal OpenFX host. The input sequence is already retimed.
#include "ofxImageEffect.h"
#include "motion.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <map>
#include <set>
#ifdef TEST_PACKAGED_OFX
#include <dlfcn.h>
#endif
#include <vector>
using namespace motion;
namespace {
OfxPlugin* plugin;
std::map<std::string,std::string> definitions;
std::vector<std::string> pageChildren;
std::set<std::string> clips;

struct Buffer{std::vector<float> v=std::vector<float>(W*H*4);};
Buffer inputs[3],intermediate[2],output;
int subframeMode=0,farReads=0;
bool extendedTimeline=false, taggedExposure=false;
Buffer outerFrames[4];
int fetched=0,released=0;
int crop[4]={0,0,W,H};
double renderTime=1.0, requested[2], angle=180, existing=0;
double look=0,capture=0,hostRate=50,projectRate=0;
char projectProps;void* instanceData=nullptr;const char* changedName="";
int curve=0,optical=0;
int capturePreset=0,lookPreset=0;
double playback=100,unmapped=50; int ramp=0; unsigned keys=0;
int diagnostic=0;char viewId;
int preview=0,draftFlag=0;char qualityId;
char rampId,speedId,cpId,lpId,lookId,captureId,source, dest, angleId, existingId, curveId, opticalId;
OfxImageEffectSuiteV1 fx{};OfxPropertySuiteV1 prop{};OfxParameterSuiteV1 params{};
int value(OfxParamHandle h,double time,...){
    va_list v;va_start(v,time);
    if((void*)h==&viewId)*va_arg(v,int*)=diagnostic;
    else if((void*)h==&qualityId)*va_arg(v,int*)=preview;
    else if((void*)h==&rampId)*va_arg(v,int*)=ramp;
    else if((void*)h==&speedId)*va_arg(v,double*)=playback;
    else if((void*)h==&cpId)*va_arg(v,int*)=capturePreset;
    else if((void*)h==&lpId)*va_arg(v,int*)=lookPreset;
    else if((void*)h==&lookId)*va_arg(v,double*)=keys?15.+time*15:look;
    else if((void*)h==&captureId)*va_arg(v,double*)=capture;
    else if((void*)h==&opticalId)*va_arg(v,int*)=optical;
    else if((void*)h==&curveId)*va_arg(v,int*)=curve;
    else *va_arg(v,double*)=((void*)h==&angleId?angle:existing);
    va_end(v);return 0;
}
std::string reported;
OfxStatus setValue(OfxParamHandle h,...){
    va_list v;va_start(v,h);reported+=va_arg(v,char*);va_end(v);return kOfxStatOK;
}
void setup(){
    params.paramSetValue=setValue;
    fx.clipDefine=[](OfxImageEffectHandle,const char* name,OfxPropertySetHandle* p){
        clips.insert(name); *p={}; return kOfxStatOK;
    };
    params.paramDefine=[](OfxParamSetHandle,const char* type,const char* name,OfxPropertySetHandle* p){
        bool inserted=definitions.emplace(name,type).second;
        *p={};return inserted?kOfxStatOK:kOfxStatErrExists;
    };
    prop.propSetString=[](OfxPropertySetHandle,const char* key,int index,const char* val){
        if(!strcmp(key,kOfxParamPropPageChild)){
            assert(index==int(pageChildren.size()));
            assert(definitions.count(val));
            pageChildren.emplace_back(val);
        }
        return kOfxStatOK;
    };
    prop.propSetInt=[](OfxPropertySetHandle,const char* key,int,int){
        // A host lacking this optional layout hint must still see all the controls.
        return !strcmp(key,kOfxParamPropGroupOpen)?kOfxStatErrUnknown:kOfxStatOK;
    };
    prop.propSetDouble=[](OfxPropertySetHandle,const char*,int,double){return kOfxStatOK;};
    fx.getPropertySet=[](OfxImageEffectHandle,OfxPropertySetHandle* p){*p=(OfxPropertySetHandle)&projectProps;return 0;};
    fx.getParamSet=[](OfxImageEffectHandle,OfxParamSetHandle* p){*p={};return 0;};
    fx.clipGetPropertySet=[](OfxImageClipHandle,OfxPropertySetHandle* p){*p={};return 0;};
    fx.clipGetHandle=[](OfxImageEffectHandle,const char* n,OfxImageClipHandle* c,OfxPropertySetHandle*){
        *c=(OfxImageClipHandle)(!strcmp(n,"Source")?&source:&dest);return 0;};
    fx.clipGetImage=[](OfxImageClipHandle c,double t,const OfxRectD*,OfxPropertySetHandle* p){
        if(t<0||t>2){
            if(!extendedTimeline || (void*)c!=&source || t!=std::floor(t) || t< -2 || t>4)return 1;
            *p=(OfxPropertySetHandle)&outerFrames[t<0?int(t)+2:int(t)-1];
            ++farReads;++fetched;return kOfxStatOK;
        }
        if(t!=int(t)){
            if(!subframeMode || (void*)c!=&source || (t!=.5 && t!=1.5))return 1;
            *p=(OfxPropertySetHandle)(subframeMode==1?&intermediate[int(t)]:&inputs[int(t)]);
            ++fetched;return 0;
        }
        *p=(OfxPropertySetHandle)((void*)c==&source?&inputs[int(t)]:&output);
        ++fetched;return 0;};
    fx.clipReleaseImage=[](OfxPropertySetHandle){++released;return 0;};
    fx.abort=[](OfxImageEffectHandle){return 0;};
    prop.propGetInt=[](OfxPropertySetHandle,const char* n,int,int* v){
        *v=!strcmp(n,kOfxImageEffectPropRenderQualityDraft)?draftFlag:!strcmp(n,kOfxImagePropRowBytes)?W*16:1;return 0;};
    prop.propGetIntN=[](OfxPropertySetHandle,const char*,int n,int* v){
        assert(n==4);std::copy(crop,crop+4,v);return 0;};
    prop.propGetDouble=[](OfxPropertySetHandle p,const char* n,int,double* v){if((void*)p==&projectProps && !strcmp(n,kOfxImageEffectPropFrameRate)){*v=projectRate;return projectRate>0?kOfxStatOK:kOfxStatErrUnknown;}*v=!strcmp(n,kOfxImageEffectPropUnmappedFrameRate)?unmapped:!strcmp(n,kOfxImageEffectPropFrameRate)?hostRate:renderTime;return 0;};
    prop.propGetDoubleN=[](OfxPropertySetHandle,const char*,int n,double* v){assert(n==2);v[0]=extendedTimeline?-2:0;v[1]=extendedTimeline?4:2;return 0;};
    prop.propSetDoubleN=[](OfxPropertySetHandle,const char*,int n,const double* v){assert(n==2);std::copy(v,v+2,requested);return 0;};
    prop.propSetPointer=[](OfxPropertySetHandle,const char*,int,void* p){instanceData=p;return 0;};
    prop.propGetPointer=[](OfxPropertySetHandle h,const char* name,int,void** p){if(!strcmp(name,kOfxPropInstanceData)){*p=instanceData;return 0;}*p=((Buffer*)h)->v.data();return 0;};
    prop.propGetString=[](OfxPropertySetHandle,const char* n,int,char** p){
        if(!strcmp(n,"OfxImageEffectPropSrcFilePath") && taggedExposure){*p=const_cast<char*>("test:50p-1/200");return kOfxStatOK;}
        *p=const_cast<char*>(!strcmp(n,kOfxPropName)?changedName:!strcmp(n,kOfxImageEffectPropComponents)?kOfxImageComponentRGBA:kOfxBitDepthFloat);return 0;};
    params.paramGetHandle=[](OfxParamSetHandle,const char* n,OfxParamHandle* h,OfxPropertySetHandle*){
        *h=(OfxParamHandle)(!strcmp(n,"diagnosticView")?&viewId:!strcmp(n,"previewQuality")?&qualityId:!strcmp(n,"ramp")?&rampId:!strcmp(n,"playbackPercent")?&speedId:!strcmp(n,"capturePreset")?&cpId:!strcmp(n,"lookPreset")?&lpId:!strcmp(n,"lookFps")?&lookId:!strcmp(n,"captureFps")?&captureId:!strcmp(n,"optical")?&opticalId:!strcmp(n,"angle")?&angleId:!strcmp(n,"existing")?&existingId:&curveId);return 0;};
    params.paramGetValueAtTime=value;
    params.paramGetNumKeys=[](OfxParamHandle,unsigned* n){*n=keys;return 0;};
    params.paramGetKeyTime=[](OfxParamHandle,unsigned i,double* t){*t=i*2.;return 0;};
}
const void* suite(OfxPropertySetHandle,const char* s,int){
    if(!strcmp(s,kOfxPropertySuite))return &prop;
    if(!strcmp(s,kOfxParameterSuite))return &params;
    return &fx;
}
void startHost(int argc,char** argv){
    setup();static OfxHost host{nullptr,suite};
#ifdef TEST_PACKAGED_OFX
    assert(argc==2);
    auto lib=dlopen(argv[1],RTLD_LOCAL|RTLD_NOW);assert(lib);
    auto get=reinterpret_cast<OfxPlugin*(*)(int)>(dlsym(lib,"OfxGetPlugin"));assert(get);
    plugin=get(1);
#else
    assert(OfxGetNumberOfPlugins()==1 && OfxGetPlugin(1)==nullptr);
    plugin=OfxGetPlugin(0);
#endif
    assert(plugin && std::string(plugin->pluginIdentifier)=="it.mecena.motion.shutter");
    plugin->setHost(&host);
    assert(plugin->mainEntry(kOfxActionLoad,nullptr,nullptr,nullptr)==0);
    assert(plugin->mainEntry(kOfxActionDescribe,nullptr,nullptr,nullptr)==kOfxStatOK);
    assert(plugin->mainEntry(kOfxImageEffectActionDescribeInContext,nullptr,nullptr,nullptr)==kOfxStatOK);
    assert(clips.count("Source") && clips.count("Output"));
    assert(definitions.at("Main")==kOfxParamTypePage);
    assert(definitions.at("fine")==kOfxParamTypeGroup);
    assert(pageChildren.size()==17);
    for(auto name:{"inspectRender","samplingInfo","renderInfo","previewQuality","ratesInfo","timelineInfo","capturePreset","lookPreset","ramp","angle","existing","lookFps","captureFps","playbackPercent","optical","curve"})
        assert(std::count(pageChildren.begin(),pageChildren.end(),name)==1);
    assert(plugin->mainEntry(kOfxActionCreateInstance,nullptr,nullptr,nullptr)==kOfxStatOK);
    assert(reported.find("sorgente Resolve")!=std::string::npos);
    std::puts("Mecena Shutter: clip/timeline clocks, look and render diagnostics, standalone instance OK");
}
}
