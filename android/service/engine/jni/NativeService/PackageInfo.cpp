#include "EngineCommon.h"
#include "PackageInfo.h"
#include "HardwareDetector.h"
#include "IOpenCVEngine.h"
#include "StringUtils.h"
#include <assert.h>
#include <vector>
#include <utils/Log.h>
#include <dlfcn.h>

using namespace std;

map<int, string> PackageInfo::InitPlatformNameMap()
{
    map<int, string> result;

    // TODO: Do not forget to add Platrfom constant to HardwareDetector.h
    result[PLATFORM_TEGRA] = PLATFORM_TEGRA_NAME;
    result[PLATFORM_TEGRA2] = PLATFORM_TEGRA2_NAME;
    result[PLATFORM_TEGRA3] = PLATFORM_TEGRA3_NAME;

    return result;
}

const map<int, string> PackageInfo::PlatformNameMap = InitPlatformNameMap();
const string PackageInfo::BasePackageName = "org.opencv.lib";
const string  DEFAULT_ENGINE_INSTALL_PATH = "/data/data/org.opencv.engine";

inline string JoinARMFeatures(int cpu_id)
{
    string result;

    if (FEATURES_HAS_NEON2 & cpu_id)
    {
    if (!((ARCH_ARMv5 & cpu_id) || (ARCH_ARMv6 & cpu_id) ||(ARCH_ARMv7 & cpu_id)))
        result = string(FEATURES_HAS_NEON2_NAME);
    }
    else if (FEATURES_HAS_NEON & cpu_id)
    {
    if (!((ARCH_ARMv5 & cpu_id) || (ARCH_ARMv6 & cpu_id)))
        result = string(FEATURES_HAS_NEON_NAME);
    }
    else if (FEATURES_HAS_VFPv3 & cpu_id)
    {
    if ((ARCH_ARMv5 & cpu_id) || (ARCH_ARMv6 & cpu_id))
        result = string(FEATURES_HAS_VFPv3_NAME);
    }
    else if (FEATURES_HAS_VFPv3d16 & cpu_id)
    {
    if ((ARCH_ARMv5 & cpu_id) || (ARCH_ARMv6 & cpu_id))
        result = string(FEATURES_HAS_VFPv3d16_NAME);
    }

    return result;
}

inline int SplitARMFeatures(const vector<string>& features)
{
    int result = 0;

    for (size_t i = 3; i < features.size(); i++)
    {
    if (FEATURES_HAS_VFPv3_NAME == features[i])
    {
        result |= FEATURES_HAS_VFPv3;
    }
    else if (FEATURES_HAS_VFPv3d16_NAME == features[i])
    {
        result |= FEATURES_HAS_VFPv3d16;
    }
    else if (FEATURES_HAS_NEON_NAME == features[i])
    {
        result |= FEATURES_HAS_NEON;
    }
    else if (FEATURES_HAS_NEON2_NAME == features[i])
    {
        result |= FEATURES_HAS_NEON2;
    }
    }

    return result;
}

inline string JoinIntelFeatures(int cpu_id)
{
    string result;

    if (FEATURES_HAS_SSSE3 & cpu_id)
    {
    result = FEATURES_HAS_SSSE3_NAME;
    }
    else if (FEATURES_HAS_SSE2 & cpu_id)
    {
    result = FEATURES_HAS_SSE2_NAME;
    }
    else if (FEATURES_HAS_SSE & cpu_id)
    {
    result = FEATURES_HAS_SSE_NAME;
    }

    return result;
}

inline int SplitIntelFeatures(const vector<string>& features)
{
    int result = 0;

    for (size_t i = 3; i < features.size(); i++)
    {
    if (FEATURES_HAS_SSSE3_NAME == features[i])
    {
        result |= FEATURES_HAS_SSSE3;
    }
    else if (FEATURES_HAS_SSE2_NAME == features[i])
    {
        result |= FEATURES_HAS_SSE2;
    }
    else if (FEATURES_HAS_SSE_NAME == features[i])
    {
        result |= FEATURES_HAS_SSE;
    }
    }

    return result;
}

inline string SplitVersion(const vector<string>& features, const string& package_version)
{
    string result;

    if ((features.size() > 1) && ('v' == features[1][0]))
    {
    result = features[1].substr(1);
    result += SplitStringVector(package_version, '.')[0];
    }
    else
    {
    // TODO: Report package name format error
    }

    return result;
}

inline string JoinPlatform(int platform)
{
    string result;
    map<int, string>::const_iterator it = PackageInfo::PlatformNameMap.find(platform);

    assert(PackageInfo::PlatformNameMap.end() != it);
    result = it->second;

    return result;
}

inline int SplitPlatfrom(const vector<string>& features)
{
    int result = 0;

    if (features.size() > 2)
    {
    string tmp = features[2];
    if (PLATFORM_TEGRA_NAME == tmp)
    {
        result = PLATFORM_TEGRA;
    }
    else if (PLATFORM_TEGRA2_NAME == tmp)
    {
        result = PLATFORM_TEGRA2;
    }
    else if (PLATFORM_TEGRA3_NAME == tmp)
    {
        result = PLATFORM_TEGRA3;
    }
    }
    else
    {
    // TODO: Report package name format error
    }

    return result;
}

/* Package naming convention
 * All parts of package name seporated by "_" symbol
 * First part is base namespace.
 * Second part is version. Version starts from "v" symbol. After "v" symbol version nomber without dot symbol added.
 * If platform is known third part is platform name
 * If platform is unknown it is defined by hardware capabilities using pattern: <arch>_<floating point and vectorization features>_<other features>
 * Example: armv7_neon, armv5_vfpv3
 */
PackageInfo::PackageInfo(const string& version, int platform, int cpu_id, std::string install_path):
    Version(version),
    Platform(platform),
    CpuID(cpu_id),
    InstallPath("")
{
#ifndef __SUPPORT_TEGRA3
    Platform = PLATFORM_UNKNOWN;
#endif
    FullName = BasePackageName + "_v" + Version.substr(0, Version.size()-1);
    if (PLATFORM_UNKNOWN != Platform)
    {
    FullName += string("_") + JoinPlatform(platform);
    }
    else
    {
    if (ARCH_UNKNOWN != CpuID)
    {
        if (ARCH_X86 & CpuID)
        {
        LOGD("PackageInfo::PackageInfo: package arch x86");
        FullName += string("_") + ARCH_X86_NAME;
#ifdef __SUPPORT_INTEL_FEATURES
        string features = JoinIntelFeatures(CpuID);
        if (!features.empty())
        {
            FullName += string("_") + features;
        }
#endif
        }
        else if (ARCH_X64 & CpuID)
        {
        LOGD("PackageInfo::PackageInfo: package arch x64");
#ifdef __SUPPORT_INTEL_x64
        FullName += string("_") + ARCH_X64_NAME;
#else
        FullName += string("_") + ARCH_X86_NAME;
#endif
#ifdef __SUPPORT_INTEL_FEATURES
        string features = JoinIntelFeatures(CpuID);
        if (!features.empty())
        {
            FullName += string("_") + features;
        }
#endif
        }
        else if (ARCH_ARMv5 & CpuID)
        {
        LOGD("PackageInfo::PackageInfo: package arch ARMv5");
        FullName += string("_") + ARCH_ARMv5_NAME;
#ifdef __SUPPORT_ARMEABI_FEATURES
        string features = JoinARMFeatures(CpuID);
        if (!features.empty())
        {
            FullName += string("_") + features;
        }
#endif
        }
        else if (ARCH_ARMv6 & CpuID)
        {
        LOGD("PackageInfo::PackageInfo: package arch ARMv6");
        // NOTE: ARM v5 used instead ARM v6
        //FullName += string("_") + ARCH_ARMv6_NAME;
        FullName += string("_") + ARCH_ARMv5_NAME;
#ifdef __SUPPORT_ARMEABI_FEATURES
        string features = JoinARMFeatures(CpuID);
        if (!features.empty())
        {
            FullName += string("_") + features;
        }
#endif
        }
        else if (ARCH_ARMv7 & CpuID)
        {
        LOGD("PackageInfo::PackageInfo: package arch ARMv7");
        FullName += string("_") + ARCH_ARMv7_NAME;
#ifdef __SUPPORT_ARMEABI_V7A_FEATURES
        string features = JoinARMFeatures(CpuID);
        if (!features.empty())
        {
            FullName += string("_") + features;
        }
#endif
        }
        else if (ARCH_ARMv8 & CpuID)
        {
        LOGD("PackageInfo::PackageInfo: package arch ARMv8");
#ifdef __SUPPORT_ARMEABI_V8
        FullName += string("_") + ARCH_ARMv8_NAME;
#else
        FullName += string("_") + ARCH_ARMv7_NAME;
#endif
        //string features = JoinARMFeatures(CpuID);
        //if (!features.empty())
        //{
        //    FullName += string("_") + features;
        //}
        }
#ifdef __SUPPORT_MIPS
        else if (ARCH_MIPS & CpuID)
        {
        FullName += string("_") + ARCH_MIPS_NAME;
        }
#endif
        else
        {
        LOGD("PackageInfo::PackageInfo: package arch unknown");
        Version.clear();
        CpuID = ARCH_UNKNOWN;
        Platform = PLATFORM_UNKNOWN;
        }
    }
    else
    {
        LOGD("PackageInfo::PackageInfo: package arch unknown");
        Version.clear();
        CpuID = ARCH_UNKNOWN;
        Platform = PLATFORM_UNKNOWN;
    }
    }

    if (!FullName.empty())
    {
    InstallPath = install_path + FullName + "/lib";
    }
}

PackageInfo::PackageInfo(const string& fullname, const string& install_path, string package_version):
    FullName(fullname),
    InstallPath(install_path)
{
    LOGD("PackageInfo::PackageInfo(\"%s\", \"%s\", \"%s\")", fullname.c_str(), install_path.c_str(), package_version.c_str());

    assert(!fullname.empty());
    assert(!install_path.empty());

    if (OPENCV_ENGINE_PACKAGE == fullname)
    {
    // Science version 1.7 OpenCV Manager has it's own version of OpenCV inside
    // Load libopencv_info.so to understand OpenCV version, platform and other features
    std::string tmp;
    if (install_path.empty())
    {
        tmp = std::string(DEFAULT_ENGINE_INSTALL_PATH) + "/" + LIB_OPENCV_INFO_NAME;
    }
    else
    {
        tmp = install_path + "/" + LIB_OPENCV_INFO_NAME;
    }

    LOGD("Trying to load info library \"%s\"", tmp.c_str());

    void* handle;
    const char* (*name_func)();
    const char* (*revision_func)();

    handle = dlopen(tmp.c_str(), RTLD_LAZY);
    if (handle)
    {
        const char* error;

        dlerror();
        *(void **) (&name_func) = dlsym(handle, "GetPackageName");
        *(void **) (&revision_func) = dlsym(handle, "GetRevision");
        error = dlerror();

        if (!error && revision_func && name_func)
        {
        FullName = std::string((*name_func)());
        package_version = std::string((*revision_func)());
        dlclose(handle);
        LOGI("OpenCV package \"%s\" revision \"%s\" found", FullName.c_str(), package_version.c_str());
        }
        else
        {
        LOGE("Library loading error (%x, %x): \"%s\"", name_func, revision_func, error);
        }
    }
    else
    {
        LOGI("Info library not found in package");
        LOGI("OpenCV Manager package does not contain any verison of OpenCV library");
        Version.clear();
        CpuID = ARCH_UNKNOWN;
        Platform = PLATFORM_UNKNOWN;
        return;
    }
    }

    vector<string> features = SplitStringVector(FullName, '_');

    if (!features.empty() && (BasePackageName == features[0]))
    {
    Version = SplitVersion(features, package_version);
    if (Version.empty())
    {
        CpuID = ARCH_UNKNOWN;
        Platform = PLATFORM_UNKNOWN;
        return;
    }

    Platform = SplitPlatfrom(features);
    if (PLATFORM_UNKNOWN != Platform)
    {
       CpuID = 0;
    }
    else
    {
        if (features.size() < 3)
        {
        LOGD("It is not OpenCV library package for this platform");
        Version.clear();
        CpuID = ARCH_UNKNOWN;
        Platform = PLATFORM_UNKNOWN;
        return;
        }
        else if (ARCH_ARMv5_NAME == features[2])
        {
        CpuID = ARCH_ARMv5 | SplitARMFeatures(features);
        }
        else if (ARCH_ARMv6_NAME == features[2])
        {
        CpuID = ARCH_ARMv6 | SplitARMFeatures(features);
        }
        else if (ARCH_ARMv7_NAME == features[2])
        {
        CpuID = ARCH_ARMv7 | SplitARMFeatures(features);
        }
        else if (ARCH_X86_NAME == features[2])
        {
        CpuID = ARCH_X86 | SplitIntelFeatures(features);
        }
        else if (ARCH_X64_NAME == features[2])
        {
        CpuID = ARCH_X64 | SplitIntelFeatures(features);
        }
#ifdef __SUPPORT_MIPS
        else if (ARCH_MIPS_NAME == features[2])
        {
        CpuID = ARCH_MIPS;
        }
#endif
        else
        {
        LOGD("It is not OpenCV library package for this platform");
        Version.clear();
        CpuID = ARCH_UNKNOWN;
        Platform = PLATFORM_UNKNOWN;
        return;
        }
    }
    }
    else
    {
    LOGD("It is not OpenCV library package for this platform");
    Version.clear();
    CpuID = ARCH_UNKNOWN;
    Platform = PLATFORM_UNKNOWN;
    return;
    }
}

bool PackageInfo::IsValid() const
{
    return !(Version.empty() && (PLATFORM_UNKNOWN == Platform) && (ARCH_UNKNOWN == CpuID));
}

int PackageInfo::GetPlatform() const
{
    return Platform;
}

int PackageInfo::GetCpuID() const
{
    return CpuID;
}

string PackageInfo::GetFullName() const
{
    return FullName;
}

string PackageInfo::GetVersion() const
{
    return Version;
}

string PackageInfo::GetInstalationPath() const
{
    return InstallPath;
}

bool PackageInfo::operator==(const PackageInfo& package) const
{
    return (package.FullName == FullName);
}