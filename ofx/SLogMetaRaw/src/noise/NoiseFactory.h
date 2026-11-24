// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ofxsImageEffect.h"

#define kNoiseIdentifier "com.slogmetaraw.SLogMetaRawNoise"

class NoiseFactory : public OFX::PluginFactoryHelper<NoiseFactory>
{
public:
    NoiseFactory();
    void load() override;
    void unload() override;
    void describe(OFX::ImageEffectDescriptor& p_Desc) override;
    void describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum p_Context) override;
    OFX::ImageEffect* createInstance(OfxImageEffectHandle p_Handle, OFX::ContextEnum p_Context) override;
};
