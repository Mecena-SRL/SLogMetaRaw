#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes fcp/LogMetaRaw.xcodeproj: an app (wrapper) and the FxPlug 4 XPC service, laid out like the Apple template.

No signing identity or team is stored in the project: pass them to xcodebuild
  xcodebuild -project LogMetaRaw.xcodeproj -scheme LogMetaRaw CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY=<id> build
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_n = [0]


def oid():
    _n[0] += 1
    return '%024X' % (0xA10000000000000000000000 + _n[0])


files = {}   # key -> (id, path, lastKnownFileType)


def fref(key, path, ftype):
    files[key] = (oid(), path, ftype)
    return files[key][0]


# ---- file references
F = {}
for key, path, t in [
    ('app_main', 'Sources/App/main.swift', 'sourcecode.swift'),
    ('p_main', 'Sources/Plugin/main.swift', 'sourcecode.swift'),
    ('p_effect', 'Sources/Plugin/DevelopEffect.swift', 'sourcecode.swift'),
    ('p_cache', 'Sources/Plugin/MetalDeviceCache.swift', 'sourcecode.swift'),
    ('p_catalog', 'Sources/Plugin/MetadataCatalog.swift', 'sourcecode.swift'),
    ('p_host_mm', 'Sources/Plugin/DevelopHost.mm', 'sourcecode.cpp.objcpp'),
    ('p_host_h', 'Sources/Plugin/DevelopHost.h', 'sourcecode.c.h'),
    ('p_types_h', 'Sources/Plugin/DevelopShaderTypes.h', 'sourcecode.c.h'),
    ('p_bridge_h', 'Sources/Plugin/Plugin-Bridging-Header.h', 'sourcecode.c.h'),
    ('p_metal', 'Sources/Plugin/Develop.metal', 'sourcecode.metal'),
    ('app_plist', 'Resources/App-Info.plist', 'text.plist.xml'),
    ('p_plist', 'Resources/Plugin-Info.plist', 'text.plist.xml'),
    ('app_ent', 'Resources/App.entitlements', 'text.plist.entitlements'),
]:
    F[key] = fref(key, path, t)

app_product = oid()
plug_product = oid()
group_root, group_products, group_sources, group_res = oid(), oid(), oid(), oid()
project_id = oid()
app_target, plug_target = oid(), oid()
app_conf_list, plug_conf_list, proj_conf_list = oid(), oid(), oid()
app_dbg, app_rel, plug_dbg, plug_rel, proj_dbg, proj_rel = (oid() for _ in range(6))
app_src_phase, app_fw_phase, app_embed_phase = oid(), oid(), oid()
plug_src_phase, plug_fw_phase, plug_script_phase = oid(), oid(), oid()
proxy, dep = oid(), oid()

bf = {}   # build file ids


def bfile(key, ref):
    bf[key] = (oid(), ref)
    return bf[key][0]


for k in ('app_main',):
    bfile('b_' + k, F[k])
for k in ('p_main', 'p_effect', 'p_cache', 'p_catalog', 'p_host_mm', 'p_metal'):
    bfile('b_' + k, F[k])
embed_bf = oid()
bf_embed = (embed_bf, plug_product)

lines = []
w = lines.append
w('// !$*UTF8*$!')
w('{ archiveVersion = 1; classes = {}; objectVersion = 56; objects = {')

w('/* Begin PBXBuildFile section */')
for key, (i, ref) in bf.items():
    name = [v[1] for k, v in files.items() if v[0] == ref][0].split('/')[-1]
    w('\t\t%s = {isa = PBXBuildFile; fileRef = %s; };' % (i, ref))
w('\t\t%s = {isa = PBXBuildFile; fileRef = %s; settings = {ATTRIBUTES = (RemoveHeadersOnCopy, ); }; };' % bf_embed)
w('/* End PBXBuildFile section */')

w('/* Begin PBXContainerItemProxy section */')
w('\t\t%s = {isa = PBXContainerItemProxy; containerPortal = %s; proxyType = 1; remoteGlobalIDString = %s; remoteInfo = LogMetaRawPlugin; };' % (proxy, project_id, plug_target))
w('/* End PBXContainerItemProxy section */')

w('/* Begin PBXCopyFilesBuildPhase section */')
w('\t\t%s = {isa = PBXCopyFilesBuildPhase; buildActionMask = 2147483647; dstPath = ""; dstSubfolderSpec = 13; files = (%s, ); name = "Embed PlugIns"; runOnlyForDeploymentPostprocessing = 0; };' % (app_embed_phase, embed_bf))
w('/* End PBXCopyFilesBuildPhase section */')

w('/* Begin PBXFileReference section */')
for key, (i, path, t) in files.items():
    w('\t\t%s = {isa = PBXFileReference; lastKnownFileType = %s; path = "%s"; sourceTree = "<group>"; };' % (i, t, path))
w('\t\t%s = {isa = PBXFileReference; explicitFileType = wrapper.application; includeInIndex = 0; path = "Log MetaRaw.app"; sourceTree = BUILT_PRODUCTS_DIR; };' % app_product)
w('\t\t%s = {isa = PBXFileReference; explicitFileType = wrapper.pluginkit; includeInIndex = 0; path = LogMetaRawPlugin.pluginkit; sourceTree = BUILT_PRODUCTS_DIR; };' % plug_product)
w('/* End PBXFileReference section */')

w('/* Begin PBXFrameworksBuildPhase section */')
w('\t\t%s = {isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0; };' % app_fw_phase)
w('\t\t%s = {isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0; };' % plug_fw_phase)
w('/* End PBXFrameworksBuildPhase section */')

w('/* Begin PBXGroup section */')
w('\t\t%s = {isa = PBXGroup; children = (%s, %s, %s); sourceTree = "<group>"; };' % (
    group_root, group_sources, group_res, group_products))
w('\t\t%s = {isa = PBXGroup; children = (%s); name = Sources; sourceTree = "<group>"; };' % (
    group_sources, ', '.join(F[k] for k in ('app_main', 'p_main', 'p_effect', 'p_cache', 'p_catalog', 'p_host_mm', 'p_host_h',
                                           'p_types_h', 'p_bridge_h', 'p_metal'))))
w('\t\t%s = {isa = PBXGroup; children = (%s); name = Resources; sourceTree = "<group>"; };' % (
    group_res, ', '.join(F[k] for k in ('app_plist', 'p_plist', 'app_ent'))))
w('\t\t%s = {isa = PBXGroup; children = (%s, %s); name = Products; sourceTree = "<group>"; };' % (
    group_products, app_product, plug_product))
w('/* End PBXGroup section */')

w('/* Begin PBXNativeTarget section */')
w('\t\t%s = {isa = PBXNativeTarget; buildConfigurationList = %s; buildPhases = (%s, %s, %s); buildRules = (); dependencies = (%s); name = LogMetaRaw; productName = LogMetaRaw; productReference = %s; productType = "com.apple.product-type.application"; };' % (
    app_target, app_conf_list, app_src_phase, app_fw_phase, app_embed_phase, dep, app_product))
w('\t\t%s = {isa = PBXNativeTarget; buildConfigurationList = %s; buildPhases = (%s, %s, %s); buildRules = (); dependencies = (); name = LogMetaRawPlugin; productName = LogMetaRawPlugin; productReference = %s; productType = "com.apple.product-type.bundle"; };' % (
    plug_target, plug_conf_list, plug_src_phase, plug_fw_phase, plug_script_phase, plug_product))
w('/* End PBXNativeTarget section */')

w('/* Begin PBXProject section */')
w('\t\t%s = {isa = PBXProject; attributes = {BuildIndependentTargetsInParallel = 1; LastSwiftUpdateCheck = 1500; LastUpgradeCheck = 1500; }; buildConfigurationList = %s; compatibilityVersion = "Xcode 14.0"; developmentRegion = en; hasScannedForEncodings = 0; knownRegions = (en, Base); mainGroup = %s; productRefGroup = %s; projectDirPath = ""; projectRoot = ""; targets = (%s, %s); };' % (
    project_id, proj_conf_list, group_root, group_products, app_target, plug_target))
w('/* End PBXProject section */')

script = ('mkdir -p \\"${TARGET_BUILD_DIR}/${FRAMEWORKS_FOLDER_PATH}\\"\\n'
          'for fw in FxPlug PluginManager; do\\n'
          '  rsync --archive --links --whole-file --no-owner --no-group --checksum --exclude=Headers --exclude=PrivateHeaders --exclude=Modules '
          '\\"/Library/Developer/Frameworks/${fw}.framework/\\" \\"${TARGET_BUILD_DIR}/${FRAMEWORKS_FOLDER_PATH}/${fw}.framework/\\" || exit 1\\n'
          '  codesign --force --timestamp=none --sign \\"${EXPANDED_CODE_SIGN_IDENTITY:--}\\" \\"${TARGET_BUILD_DIR}/${FRAMEWORKS_FOLDER_PATH}/${fw}.framework\\"\\n'
          'done\\n')
w('/* Begin PBXShellScriptBuildPhase section */')
w('\t\t%s = {isa = PBXShellScriptBuildPhase; buildActionMask = 2147483647; files = (); inputPaths = (); name = "Copy and sign FxPlug frameworks"; outputPaths = (); runOnlyForDeploymentPostprocessing = 0; shellPath = /bin/sh; shellScript = "%s"; };' % (plug_script_phase, script))
w('/* End PBXShellScriptBuildPhase section */')

w('/* Begin PBXSourcesBuildPhase section */')
w('\t\t%s = {isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = (%s); runOnlyForDeploymentPostprocessing = 0; };' % (
    app_src_phase, bf['b_app_main'][0]))
w('\t\t%s = {isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = (%s); runOnlyForDeploymentPostprocessing = 0; };' % (
    plug_src_phase, ', '.join(bf['b_' + k][0] for k in ('p_main', 'p_effect', 'p_cache', 'p_catalog', 'p_host_mm', 'p_metal'))))
w('/* End PBXSourcesBuildPhase section */')

w('/* Begin PBXTargetDependency section */')
w('\t\t%s = {isa = PBXTargetDependency; target = %s; targetProxy = %s; };' % (dep, plug_target, proxy))
w('/* End PBXTargetDependency section */')

common = {
    'SDKROOT': 'macosx', 'MACOSX_DEPLOYMENT_TARGET': '14.6', 'SWIFT_VERSION': '5.0',
    'CLANG_ENABLE_OBJC_ARC': 'YES', 'CLANG_CXX_LANGUAGE_STANDARD': '"c++17"', 'CLANG_CXX_LIBRARY': '"libc++"',
    'ENABLE_USER_SCRIPT_SANDBOXING': 'NO', 'ONLY_ACTIVE_ARCH': 'NO', 'MTL_FAST_MATH': 'NO',
    'MTL_LANGUAGE_REVISION': 'Metal30', 'GCC_C_LANGUAGE_STANDARD': 'gnu17',
}


def settings(d):
    return '{' + ' '.join('%s = %s;' % (k, v) for k, v in d.items()) + ' }'


w('/* Begin XCBuildConfiguration section */')
for i, name, extra in ((proj_dbg, 'Debug', {'SWIFT_OPTIMIZATION_LEVEL': '"-Onone"', 'GCC_OPTIMIZATION_LEVEL': '0'}),
                       (proj_rel, 'Release', {'SWIFT_COMPILATION_MODE': 'wholemodule', 'GCC_OPTIMIZATION_LEVEL': 's'})):
    w('\t\t%s = {isa = XCBuildConfiguration; buildSettings = %s; name = %s; };' % (i, settings({**common, **extra}), name))
app_s = {'PRODUCT_NAME': '"Log MetaRaw"', 'PRODUCT_BUNDLE_IDENTIFIER': 'com.slogmetaraw.fcp.LogMetaRaw',
         'INFOPLIST_FILE': 'Resources/App-Info.plist', 'CODE_SIGN_ENTITLEMENTS': 'Resources/App.entitlements',
         'ENABLE_HARDENED_RUNTIME': 'NO', 'GENERATE_INFOPLIST_FILE': 'NO', 'SKIP_INSTALL': 'NO'}
plug_s = {'PRODUCT_NAME': 'LogMetaRawPlugin', 'PRODUCT_BUNDLE_IDENTIFIER': 'com.slogmetaraw.fcp.LogMetaRaw.Plugin',
          'INFOPLIST_FILE': 'Resources/Plugin-Info.plist', 'WRAPPER_EXTENSION': 'pluginkit',
          'MACH_O_TYPE': 'mh_execute', 'GENERATE_INFOPLIST_FILE': 'NO', 'SKIP_INSTALL': 'YES',
          'ENABLE_HARDENED_RUNTIME': 'NO', 'LD_RUNPATH_SEARCH_PATHS': '"$(inherited) @loader_path/../Frameworks"',
          'SWIFT_OBJC_BRIDGING_HEADER': '"Sources/Plugin/Plugin-Bridging-Header.h"',
          'FRAMEWORK_SEARCH_PATHS': '"/Library/Developer/SDKs/FxPlug.sdk/Library/Frameworks"',
          'HEADER_SEARCH_PATHS': '"$(SRCROOT)/Sources/Plugin"',
          'OTHER_LDFLAGS': '"-framework FxPlug -framework PluginManager -framework Foundation -framework AppKit -framework Metal -framework Accelerate"',
          'CODE_SIGN_INJECT_BASE_ENTITLEMENTS': 'NO'}
for i, name, s in ((app_dbg, 'Debug', app_s), (app_rel, 'Release', app_s), (plug_dbg, 'Debug', plug_s), (plug_rel, 'Release', plug_s)):
    w('\t\t%s = {isa = XCBuildConfiguration; buildSettings = %s; name = %s; };' % (i, settings(s), name))
w('/* End XCBuildConfiguration section */')

w('/* Begin XCConfigurationList section */')
for i, a, b, n in ((proj_conf_list, proj_dbg, proj_rel, 'PBXProject'), (app_conf_list, app_dbg, app_rel, 'PBXNativeTarget'),
                   (plug_conf_list, plug_dbg, plug_rel, 'PBXNativeTarget')):
    w('\t\t%s = {isa = XCConfigurationList; buildConfigurations = (%s, %s); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release; };' % (i, a, b))
w('/* End XCConfigurationList section */')
w('}; rootObject = %s; }' % project_id)

proj_dir = os.path.join(HERE, 'LogMetaRaw.xcodeproj')
os.makedirs(proj_dir, exist_ok=True)
with open(os.path.join(proj_dir, 'project.pbxproj'), 'w') as fh:
    fh.write('\n'.join(lines) + '\n')

# a shared scheme so that "-scheme LogMetaRaw" builds the app and, through the dependency, the plug-in
sd = os.path.join(proj_dir, 'xcshareddata', 'xcschemes')
os.makedirs(sd, exist_ok=True)
with open(os.path.join(sd, 'LogMetaRaw.xcscheme'), 'w') as fh:
    fh.write('''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1500" version="1.7">
   <BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES">
      <BuildActionEntries>
         <BuildActionEntry buildForTesting="NO" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES">
            <BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="%s" BuildableName="Log MetaRaw.app" BlueprintName="LogMetaRaw" ReferencedContainer="container:LogMetaRaw.xcodeproj"/>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <LaunchAction buildConfiguration="Release" selectedDebuggerIdentifier="" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.PosixSpawn" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" allowLocationSimulation="YES"/>
</Scheme>
''' % app_target)
print('scritto', proj_dir)
