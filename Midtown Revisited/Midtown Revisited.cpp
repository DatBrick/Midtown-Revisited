
#include "stdafx.h"
#include "Midtown Revisited.h"

#include <ctime>
#include <valarray>
#include <cstdio>
#include <cstdint>
#include <utility>
#include <vector>

#define DIRECTX_VERSION 0x0700

#ifdef DIRECTX_VERSION
# ifndef DIRECT3D_VERSION
#  define DIRECT3D_VERSION DIRECTX_VERSION
# endif
# ifndef DIRECTDRAW_VERSION
#  define DIRECTDRAW_VERSION DIRECTX_VERSION
# endif
# ifndef DIRECTINPUT_VERSION
#  define DIRECTINPUT_VERSION DIRECTX_VERSION
# endif
# ifndef DIRECTSOUND_VERSION
#  define DIRECTSOUND_VERSION DIRECTX_VERSION
# endif
#endif

#include <d3d.h>
#include <ddraw.h>
#include <dinput.h>

#include "memHandle.h"

#define ASSERT_SIZE(type, size) static_assert(sizeof(type) == size, "Invalid "#type" size");

template <typename... T>
inline void DebugPrint(const char* format, T&&... args)
{
    char buffer[2048];
    std::snprintf(buffer, std::size(buffer), format, std::forward<T>(args)...);

    OutputDebugStringA(std::string("[Midtown Revisited] | ").append(buffer).append("\n").c_str());
}

enum class HookType
{
    JMP,
    CALL,

    COUNT,
};

const char* HookTypeNames[static_cast<std::size_t>(HookType::COUNT)] =
{
    "JMP",
    "CALL",
};

void CreateHook(const char* name, const char* description, memHandle pHook, memHandle pDetour, HookType type)
{
    std::intptr_t RVA = pDetour.as<std::intptr_t>() - pHook.offset(5).as<std::intptr_t>();

    switch (type)
    {
        case HookType::JMP:
        {
            unsigned char buffer[5] = { 0xE9 };
            reinterpret_cast<int&>(buffer[1]) = RVA;

            memProtect(pHook, sizeof(buffer), true, true, true).memcpy(buffer);
        } break;

        case HookType::CALL:
        {
            unsigned char buffer[5] = { 0xE8 };
            reinterpret_cast<int&>(buffer[1]) = RVA;

            memProtect(pHook, sizeof(buffer), true, true, true).memcpy(buffer);
        } break;
    }

    DebugPrint(
        "Created %s %s hook at 0x%X pointing to 0x%X - %s",
        name,
        HookTypeNames[static_cast<std::size_t>(type)],
        pHook.as<std::uintptr_t>(),
        pDetour.as<std::uintptr_t>(),
        description
    );
}

void CreatePatch(const char* name, const char* description, memHandle dest, memHandle src, std::size_t size)
{
    memProtect(dest, size, true, true, true).memcpy(src);

    DebugPrint(
        "Created %s patch at 0x%X of size %zu - %s",
        name,
        dest.as<std::uintptr_t>(),
        size,
        description
    );
}

struct gfxInterface
{
    enum gfxDeviceType
    {
        Software        = 0, // Software (No 3D Video Card)
        Hardware        = 1, // Hardware (3D Video Card)
        HardwareWithTnL = 2  // Hardware (3D Video Card With T&L)
    };

    enum gfxDepthFlags
    {
        Depth16 = 0x400,
        Depth24 = 0x200,
        Depth32 = 0x100
    };

    struct gfxResData
    {
        unsigned short ScreenWidth;
        unsigned short ScreenHeight;
        unsigned short ColorDepth;
        unsigned short Flags; // = 0b110 | (ColorDepth == 16)
    };

    GUID GUID;
    char Name[64];

    unsigned int DeviceCaps;

    gfxDeviceType DeviceType;

    unsigned int ResolutionCount;   // Max of 64 resolutions
    unsigned int ResolutionChoice;

    gfxDepthFlags AcceptableDepths;  // Used to check if mmResolution::Depth is allowed

    unsigned int AvailableMemory;
    unsigned int VendorID;
    unsigned int DeviceID;

    gfxResData Resolutions[64];
};

ASSERT_SIZE(gfxInterface, 624);

// AGEHook<(0x[0-9A-F]{6})>::Type<(.+)> (.+);
// auto& $3 = memHandle($1).as<$2&>();\n

auto& sm_EnableSetLOD = memHandle(0x684D34).as<bool&>();
auto& sm_Allow32 = memHandle(0x684D36).as<bool&>();
auto& hWndParent = memHandle(0x682FA0).as<HWND&>();
auto& hWndMain = memHandle(0x6830B8).as<HWND&>();
auto& lpWindowTitle = memHandle(0x68311C).as<LPCSTR&>();
auto& ATOM_Class = memHandle(0x6830F0).as<ATOM&>();
auto& IconID = memHandle(0x683108).as<LPCSTR&>();
auto& pageFlip = memHandle(0x5CA3EC).as<bool&>();
auto& hasBorder = memHandle(0x5CA3ED).as<bool&>();
auto& useMultiTexture = memHandle(0x5CA3EE).as<bool&>();
auto& enableHWTnL = memHandle(0x5CA664).as<bool&>();
auto& novblank = memHandle(0x68451D).as<bool&>();
auto& inWindow = memHandle(0x6830D0).as<bool&>();
auto& isMaximized = memHandle(0x6830D1).as<bool&>();
auto& tripleBuffer = memHandle(0x6830D2).as<bool&>();
auto& useReference = memHandle(0x6830D3).as<bool&>();
auto& useSoftware = memHandle(0x6830D4).as<bool&>();
auto& useAgeSoftware = memHandle(0x6830D5).as<bool&>();
auto& useBlade = memHandle(0x6830D6).as<bool&>();
auto& useSysMem = memHandle(0x6830D7).as<bool&>();
auto& useInterface = memHandle(0x6830D8).as<int&>();
auto& lpDirectDrawCreateEx = memHandle(0x684518).as<decltype(&DirectDrawCreateEx)&>();
auto& lpDD = memHandle(0x6830A8).as<IDirectDraw7 *&>();
auto& lpD3D = memHandle(0x6830AC).as<IDirect3D7 *&>();
auto& lpD3DDev = memHandle(0x6830C8).as<IDirect3DDevice7 *&>();
auto& lpdsRend = memHandle(0x6830CC).as<IDirectDrawSurface7 *&>();
auto& gfxInterfaces = memHandle(0x683130).as<gfxInterface[8]>();
auto& gfxInterfaceCount = memHandle(0x6844C0).as<unsigned int&>();
auto& gfxInterfaceChoice = memHandle(0x6844C8).as<int&>();
auto& gfxMinScreenWidth = memHandle(0x6844B0).as<int&>();
auto& gfxMinScreenHeight = memHandle(0x6844CC).as<int&>();
auto& gfxMaxScreenWidth = memHandle(0x6844FC).as<int&>();
auto& gfxMaxScreenHeight = memHandle(0x6844D8).as<int&>();
auto& gfxTexQuality = memHandle(0x6B165C).as<int&>();
auto& gfxTexReduceSize = memHandle(0x6857D0).as<int&>();
auto& window_fWidth = memHandle(0x6830F4).as<float&>();
auto& window_fHeight = memHandle(0x683120).as<float&>();
auto& window_iWidth = memHandle(0x683128).as<int&>();
auto& window_iHeight = memHandle(0x683100).as<int&>();
auto& window_ZDepth = memHandle(0x6830E4).as<int&>();
auto& window_ColorDepth = memHandle(0x6830F8).as<int&>();
auto& window_X = memHandle(0x6830EC).as<int&>();
auto& window_Y = memHandle(0x683110).as<int&>();
auto& ioMouse_InvWidth = memHandle(0x6A38EC).as<float&>();
auto& ioMouse_InvHeight = memHandle(0x6A38D4).as<float&>();


auto& StringPrinter = memHandle(0x5CECF0).as<void(*&)(LPCSTR)>();
auto& Printer = memHandle(0x5CED24).as<void(*&)(int, LPCSTR, va_list)>();
auto& FatalErrorHandler = memHandle(0x6A3D38).as<void(*&)(void)>();
auto& gameClosing = memHandle(0x6B1708).as<BOOL&>();
auto& gameState = memHandle(0x6B17C8).as<int&>();
auto& joyDebug = memHandle(0x6A3AA8).as<int&>();
auto& assetDebug = memHandle(0x6A3C0C).as<int&>();
auto& gfxDebug = memHandle(0x683104).as<int&>();
auto& audDebug = memHandle(0x6B4C24).as<int&>();


auto gfxWindowProc = memHandle(0x4A88F0).as<WNDPROC>();
auto DoesArgExist  = memHandle(0x4C6190).as<bool(*)(const char* name)>();
auto GetArgInt     = memHandle(0x4C61C0).as<bool(*)(const char* name, unsigned int index, int& out)>();
auto GetArgFloat   = memHandle(0x4C6210).as<bool(*)(const char* name, unsigned int index, float& out)>();
auto GetArgString  = memHandle(0x4C6260).as<bool(*)(const char* name, unsigned int index, const char*& out)>();

auto& obj_NoDrawThresh = memHandle(0x5C571C).as<float&>();
auto& obj_VLowThresh = memHandle(0x5C6658).as<float&>();
auto& obj_LowThresh = memHandle(0x5C665C).as<float&>();
auto& obj_MedThresh = memHandle(0x5C6660).as<float&>();
auto& sdl_VLowThresh = memHandle(0x5C5708).as<float&>();
auto& sdl_LowThresh = memHandle(0x5C570C).as<float&>();
auto& sdl_MedThresh = memHandle(0x5C5710).as<float&>();
// auto& ROOT = memHandle(0x661738).as<asNode&>();
auto& cityName = memHandle(0x6B167C).as<char[40]>();
auto& cityName2 = memHandle(0x6B16A4).as<char[40]>();
auto& timeOfDay = memHandle(0x62B068).as<int&>();
auto& vehCar_bHeadlights = memHandle(0x627518).as<int&>();


void gfxWindowMove(bool isOpen)
{
    HDC hDC = GetDC(NULL);
    int screenWidth  = GetDeviceCaps(hDC, HORZRES);
    int screenHeight = GetDeviceCaps(hDC, VERTRES);
    ReleaseDC(0, hDC);

    window_X = (screenWidth  - window_iWidth)  / 2;
    window_Y = (screenHeight - window_iHeight) / 2;

    if (isOpen)
    {
        MoveWindow(hWndMain, window_X, window_Y, window_iWidth, window_iHeight, 0);
    }
}

void gfxWindowUpdate(bool isOpen)
{
    RECT rect;
    GetClientRect(hWndMain, &rect);

    MoveWindow(hWndMain,window_X, window_Y, (2 * window_iWidth - rect.right), (2 * window_iHeight - rect.bottom), isOpen);
}

void gfxWindowCreate(LPCSTR lpWindowName)
{
    if (hWndMain)
    {
        return;
    }

    if (lpWindowTitle)
    {
        lpWindowName = lpWindowTitle;
    }

    if (ATOM_Class == NULL)
    {
        WNDCLASSA wc =
        {
            CS_HREDRAW | CS_VREDRAW,    /* style */
            gfxWindowProc,              /* lpfnWndProc */
            0,                          /* cbClsExtra */
            0,                          /* cbWndExtra */
            0,                          /* hInstance */
            LoadIconA(GetModuleHandleA(NULL), IconID ? IconID : IDI_APPLICATION),
                                        /* hIcon */
            LoadCursorA(0, IDC_ARROW),  /* hCursor */
            CreateSolidBrush(NULL),     /* hbrBackground */
            NULL,                       /* lpszMenuName */
            "gfxWindow",                /* lpszClassName */
        };

        ATOM_Class = RegisterClassA(&wc);
    }

    DWORD dwStyle = WS_POPUP;

    if (inWindow)
    {
        if (hWndParent)
        {
            dwStyle = WS_CHILD;
        }
        else if (hasBorder = !(DoesArgExist("noborder") || DoesArgExist("borderless")))
        {
            dwStyle |= (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
        }
    }
    else
    {
        dwStyle |= WS_SYSMENU;
    }

    // update the position
    gfxWindowMove(false);

    hWndMain = CreateWindowExA(
        WS_EX_APPWINDOW,
        "gfxWindow",
        lpWindowName,
        dwStyle,
        window_X,
        window_Y,
        640,
        480,
        hWndParent,
        0,
        0,
        0);

    if (inWindow)
    {
        gfxWindowUpdate(false);
    }

    SetCursor(NULL);
    ShowCursor(FALSE);

    ShowWindow(hWndMain, TRUE);
    UpdateWindow(hWndMain);
    SetFocus(hWndMain);
}

static void SetRes(int width, int height, int cdepth, int zdepth, bool parseArgs) {
    if (DoesArgExist("ref")) {
        useSoftware    = 1;
        useReference   = 1;
    } else if (DoesArgExist("blade") || DoesArgExist("bladed")) {
        useSoftware    = 1;
        useBlade       = 1;
    } else if (DoesArgExist("swage")) {
        useSoftware    = 1;
        useAgeSoftware = 1;
    } else if (DoesArgExist("sw")) {
        useSoftware    = 1;
    }

    if (DoesArgExist("sysmem")) {
        useSysMem = 1;
    }
    if (DoesArgExist("triple")) {
        tripleBuffer = 1;
    }

    if (DoesArgExist("nomultitexture") || DoesArgExist("nomt")) {
        useMultiTexture = 0;
    }
    if (DoesArgExist("novblank") || DoesArgExist("novsync")) {
        novblank = 1;
    }
    if (DoesArgExist("nohwtnl")) {
        enableHWTnL = 0;
    }

    if (DoesArgExist("primary")) {
        useInterface = 0;
    } else {
        GetArgInt("display", 0, useInterface);
    }
    if (DoesArgExist("single")) {
        pageFlip = 0;
    }

    if (DoesArgExist("window") || DoesArgExist("windowed")) {
        inWindow = 1;
    } else if (DoesArgExist("fs") || DoesArgExist("fullscreen")) {
        inWindow = 0;
    }

    int bitDepth = 0;
    if (GetArgInt("bpp", 0, bitDepth) || GetArgInt("bitdepth", 0, bitDepth)) {
        cdepth = bitDepth;
        zdepth = bitDepth;
    } else {
        GetArgInt("cdepth", 0, cdepth);
        GetArgInt("zdepth", 0, zdepth);
    }

    // We don't want to set the width/height if we are in a menu, it just fucks it up
    if (gameState != 0) {
        if (DoesArgExist("max")) {
            HDC hDC = GetDC(NULL);
            width  = GetDeviceCaps(hDC, HORZRES);
            height = GetDeviceCaps(hDC, VERTRES);
            ReleaseDC(0, hDC);
        } else {
            GetArgInt("width",  0, width);
            GetArgInt("height", 0, height);
        }

        // DoesArgExist("width",  0, &width);
        // DoesArgExist("height", 0, &height);
    }

    useSysMem = useSoftware;

    window_iWidth  = width;
    window_iHeight = height;

    window_fWidth  = float(width);
    window_fHeight = float(height);

    window_ColorDepth = cdepth;
    window_ZDepth     = zdepth;

    sm_Allow32 = (cdepth == 32);

    if (lpDD)
    {
        if (inWindow)
        {
            gfxWindowMove(true);
            gfxWindowUpdate(true);
        } else {
            DDSURFACEDESC2 ddSurfaceDesc;

            ddSurfaceDesc.dwSize = 0x7C;

            if ((lpDD->GetDisplayMode(&ddSurfaceDesc) != DD_OK) || (ddSurfaceDesc.dwWidth != window_iWidth) || (ddSurfaceDesc.dwHeight != window_iHeight))
            {
                if (lpDD->SetDisplayMode(
                    window_iWidth,
                    window_iHeight,
                    window_ColorDepth,
                    0,
                    0) != DD_OK) {
                    DebugPrint("[gfxPipeline::SetRes]: SHIT! Failed to set the display mode!");
                }
            }
        }
    }

    ioMouse_InvWidth  = (1.0f / window_fWidth);
    ioMouse_InvHeight = (1.0f / window_fHeight);
}

auto $DeviceCallback = memHandle(0x4AC3D0).as<LPD3DENUMDEVICESCALLBACK7>();
auto $ResCallback    = memHandle(0x4AC6F0).as<LPDDENUMMODESCALLBACK2>();

BOOL __stdcall AutoDetectCallback(GUID *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext)
{
    if (lpDirectDrawCreateEx(lpGUID, (LPVOID*)&lpDD, IID_IDirectDraw7, nullptr) == DD_OK)
    {
        gfxInterface *gfxInterface = &gfxInterfaces[gfxInterfaceCount++];

        std::strncpy(gfxInterface->Name, lpDriverDescription, std::size(gfxInterface->Name));

        gfxInterface->DeviceCaps = 1;
        gfxInterface->AcceptableDepths = gfxInterface::gfxDepthFlags::Depth32;

        DDDEVICEIDENTIFIER2 ddDeviceIdentifier = { NULL };

        if (lpDD->GetDeviceIdentifier(&ddDeviceIdentifier, 0) == DD_OK)
        {
            gfxInterface->VendorID  = ddDeviceIdentifier.dwVendorId;
            gfxInterface->DeviceID  = ddDeviceIdentifier.dwDeviceId;
            gfxInterface->GUID      = ddDeviceIdentifier.guidDeviceIdentifier;
        }

        if (lpDD->QueryInterface(IID_IDirect3D7, (LPVOID*)&lpD3D) == DD_OK)
        {
            lpD3D->EnumDevices($DeviceCallback, gfxInterface);
            lpD3D->Release();

            lpD3D = nullptr;
        }

        gfxInterface->DeviceType        = gfxInterface::gfxDeviceType::HardwareWithTnL;

        gfxInterface->ResolutionCount   = 0;
        gfxInterface->ResolutionChoice  = 0;

        DWORD availableMemory = 0x40000000; // 1GB = 1024 * 1024 * 1024

        DDSCAPS2 ddsCaps = { NULL };

        ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;

        if (lpDD->GetAvailableVidMem(&ddsCaps, &availableMemory, NULL) != DD_OK)
        {
            DebugPrint("Couldn't get video memory, using default");
        }

        DebugPrint("Total video memory: %dMB", (availableMemory >> 20));

        gfxInterface->AvailableMemory = availableMemory;

        gfxMaxScreenWidth = 0;
        gfxMaxScreenHeight = 0;

        lpDD->EnumDisplayModes(0, 0, gfxInterface, $ResCallback);
        lpDD->Release();

        lpDD = nullptr;
    }

    return TRUE;
}

class memSafeHeap;

auto $memSafeHeap_Init = memHandle(0x577210).as<void(memSafeHeap::*)(void*, unsigned int, bool, bool, bool)>();

class memSafeHeap
{
public:
    void Init(void *memAllocator, unsigned int heapSize, bool p3, bool p4, bool checkAlloc)
    {
        int heapSizeMB = 128;
        GetArgInt("heapsize", 0, heapSizeMB);

        heapSize = heapSizeMB * (1024 * 1024);

        DebugPrint("[memSafeHeap::Init]: Allocating %dMB heap (%d bytes)\n", heapSizeMB, heapSize);
        return (this->*$memSafeHeap_Init)(memAllocator, heapSize, p3, p4, checkAlloc);
    }
};

auto $mmDirSnd_Init = memHandle(0x51CC50).as<void*(*)(int, bool, int, float, const char*, bool)>();

auto& CurrentAudioDevice = memHandle(0x6B17F2).as<char[200]>();

void* mmDirSnd_Init(int sampleRate, bool enableStero, int a4, float volume, const char* deviceName, bool enable3D)
{
    if (deviceName[0] == '\0')
    {
        std::strncpy(CurrentAudioDevice, "Primary Sound Driver", std::size(CurrentAudioDevice));

        deviceName = CurrentAudioDevice;

        DebugPrint("[mmDirSnd::Init]: Using Primary Sound Driver");
    }

    /*
        TODO:

        - Set sampling rate (see: AudManager::SetBitDepthAndSampleRate(int bitDepth, ulong samplingRate))
        - Redo SetPrimaryBufferFormat to set sampleSize? (see: DirSnd::SetPrimaryBufferFormat(ulong sampleRate, bool allowStero))
    */
    return $mmDirSnd_Init(48000, enableStero, a4, volume, deviceName, enable3D);
}

auto& __VtResumeSampling = memHandle(0x5E0CC4).as<void(*&)(void)>();
auto& __VtPauseSampling  = memHandle(0x5E0CD8).as<void(*&)(void)>();

void Initialize()
{
    if (std::strcmp(memHandle(0x5C28FC).as<const char*>(), "Angel: 3393 / Nov  3 2000 14:34:22") != 0)
    {
        DebugPrint("Unknown MM2 Version Detected");

        return;
    }

    DebugPrint("Initialization Begin");

    std::clock_t begin = std::clock();

    CreateHook("gfxPipeline::SetRes",          "Custom implementation allowing for more control of the window",                0x4A8CE0, &SetRes,             HookType::JMP);
    CreateHook("AutoDetectCallback",           "Replaces the default AutoDetect method with a much faster one",                0x4AC030, &AutoDetectCallback, HookType::JMP);
    CreateHook("memSafeHeap::Init",            "Adds '-heapsize' parameter that takes a size in megabytes. Defaults to 128MB", 0x4015DD, &memSafeHeap::Init,  HookType::CALL);
    CreateHook("gfxPipeline::gfxWindowCreate", "Custom implementation allowing for more control of the windo.",                0x4A8A90, &gfxWindowCreate,    HookType::JMP);
    CreateHook("mmDirSnd::Init",               "Fixes no sound issue on startup.",                                             0x51941D, &mmDirSnd_Init,      HookType::CALL);

    CreatePatch("sfPointer::Update", "Enables pointer in windowed mode", 0x4F136E, "\x90\x90", 2);

    DebugPrint("Initialize Completed in %.2f Seconds", double(std::clock() - begin) / CLOCKS_PER_SEC);
}
