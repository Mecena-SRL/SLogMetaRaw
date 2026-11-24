// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ofxsImageEffect.h"

// The Detail's slot and identifier: a project saved with the Detail opens with Creative.
#define kCreativeIdentifier "com.slogmetaraw.SLogMetaRawDetail"

class CreativeFactory : public OFX::PluginFactoryHelper<CreativeFactory>
{
public:
    CreativeFactory();
    void load() override;
    void unload() override;
    void describe(OFX::ImageEffectDescriptor& p_Desc) override;
    void describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum p_Context) override;
    OFX::ImageEffect* createInstance(OfxImageEffectHandle p_Handle, OFX::ContextEnum p_Context) override;
};
