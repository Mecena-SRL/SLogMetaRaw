#include "ofxImageEffect.h"
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
int main(int argc,char** argv){
    if(argc!=2)return 1;
    auto lib=dlopen(argv[1],RTLD_LOCAL|RTLD_NOW);
    if(!lib){std::fprintf(stderr,"%s\n",dlerror());return 2;}
    auto count=reinterpret_cast<int(*)()>(dlsym(lib,"OfxGetNumberOfPlugins"));
    auto get=reinterpret_cast<OfxPlugin*(*)(int)>(dlsym(lib,"OfxGetPlugin"));
    if(!get||!count||count()!=2||get(-1)||get(2))return 3;
    const char* ids[]={"it.mecena.motion.stabilize","it.mecena.motion.shutter"};
    for(int i=0;i<2;++i){
        auto p=get(i);
        if(!p||!p->pluginIdentifier||!p->setHost||!p->mainEntry||std::strcmp(ids[i],p->pluginIdentifier))return 4;
        std::puts(p->pluginIdentifier);
    }
}
