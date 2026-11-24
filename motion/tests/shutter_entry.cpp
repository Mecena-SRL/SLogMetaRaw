// Standalone descriptor adapter for the Linux/macOS shutter host regression.
#include "ofxImageEffect.h"
#include <cstring>
extern "C" OfxPlugin* mecena_shutter_descriptor();
OfxExport int OfxGetNumberOfPlugins(){return 1;}
OfxExport OfxPlugin* OfxGetPlugin(int n){return n==0?mecena_shutter_descriptor():nullptr;}
struct SourceRates { double capture, file, exposureMs; };
extern "C" SourceRates mecena_source_rates(const char* path) {
    return path && !std::strcmp(path,"test:50p-1/200")?SourceRates{50,50,5}:SourceRates{};
}
extern "C" const char* mecena_source_path_property() { return "OfxImageEffectPropSrcFilePath"; }
