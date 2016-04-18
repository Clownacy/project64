/****************************************************************************
*                                                                           *
* Project64 - A Nintendo 64 emulator.                                      *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/
#include "stdafx.h"
#include <Project64-core/N64System/SystemGlobals.h>
#include <Project64-core/N64System/N64RomClass.h>
#include <Project64-core/N64System/Mips/MemoryVirtualMem.h>
#include <Project64-core/N64System/Mips/RegisterClass.h>
#include "GFXPlugin.h"

CGfxPlugin::CGfxPlugin() :
CaptureScreen(NULL),
ChangeWindow(NULL),
DrawScreen(NULL),
DrawStatus(NULL),
MoveScreen(NULL),
ProcessDList(NULL),
ProcessRDPList(NULL),
ShowCFB(NULL),
UpdateScreen(NULL),
ViStatusChanged(NULL),
ViWidthChanged(NULL),
SoftReset(NULL),
GetRomBrowserMenu(NULL),
OnRomBrowserMenuItem(NULL),
GetDebugInfo(NULL),
InitiateDebugger(NULL)
{
    memset(&m_GFXDebug, 0, sizeof(m_GFXDebug));
}

CGfxPlugin::~CGfxPlugin()
{
    WriteTrace(TraceGFXPlugin, TraceDebug, "Start");
    Close(NULL);
    UnloadPlugin();
    WriteTrace(TraceGFXPlugin, TraceDebug, "Done");
}

bool CGfxPlugin::LoadFunctions(void)
{
    // Find entries for functions in DLL
    int32_t(CALL *InitiateGFX) (void * Gfx_Info);
    LoadFunction(InitiateGFX);
    LoadFunction(ChangeWindow);
    LoadFunction(DrawScreen);
    LoadFunction(MoveScreen);
    LoadFunction(ProcessDList);
    LoadFunction(UpdateScreen);
    LoadFunction(ViStatusChanged);
    LoadFunction(ViWidthChanged);
    LoadFunction(SoftReset);
#ifdef ANDROID
    LoadFunction(SurfaceCreated);
    LoadFunction(SurfaceChanged);
#endif

    // version 0x104 functions
    _LoadFunction("DrawFullScreenStatus", DrawStatus);

    // Rom Browser
    LoadFunction(GetRomBrowserMenu);
    LoadFunction(OnRomBrowserMenuItem);

    //Make sure dll had all needed functions
    if (ChangeWindow == NULL)    { UnloadPlugin(); return false; }
    if (DrawScreen == NULL)      { DrawScreen = DummyDrawScreen; }
    if (InitiateGFX == NULL)     { UnloadPlugin(); return false; }
    if (MoveScreen == NULL)      { MoveScreen = DummyMoveScreen; }
    if (ProcessDList == NULL)    { UnloadPlugin(); return false; }
    if (UpdateScreen == NULL)    { UnloadPlugin(); return false; }
    if (ViStatusChanged == NULL) { ViStatusChanged = DummyViStatusChanged; }
    if (ViWidthChanged == NULL)  { ViWidthChanged = DummyViWidthChanged; }
    if (SoftReset == NULL)       { SoftReset = DummySoftReset; }

    if (m_PluginInfo.Version >= 0x0103)
    {
        LoadFunction(ProcessRDPList);
        LoadFunction(CaptureScreen);
        LoadFunction(ShowCFB);
        LoadFunction(GetDebugInfo);
        _LoadFunction("InitiateGFXDebugger", InitiateDebugger);

        if (ProcessRDPList == NULL) { UnloadPlugin(); return false; }
        if (CaptureScreen == NULL)  { UnloadPlugin(); return false; }
        if (ShowCFB == NULL)        { UnloadPlugin(); return false; }
    }

    if (m_PluginInfo.Version >= 0x0104)
    {
        if (PluginOpened == NULL) { UnloadPlugin(); return false; }
    }

    if (GetDebugInfo != NULL)
    {
        GetDebugInfo(&m_GFXDebug);
    }
    return true;
}

bool CGfxPlugin::Initiate(CN64System * System, RenderWindow * Window)
{
    WriteTrace(TraceGFXPlugin, TraceDebug, "Starting");
    if (m_Initialized)
    {
        Close(Window);
    }

    typedef struct
    {
        void * hWnd;			/* Render window */
        void * hStatusBar;    /* if render window does not have a status bar then this is NULL */

        int32_t MemoryBswaped;    // If this is set to TRUE, then the memory has been pre
        //   bswap on a dword (32 bits) boundry
        //	eg. the first 8 bytes are stored like this:
        //        4 3 2 1   8 7 6 5

        uint8_t * HEADER;	// This is the rom header (first 40h bytes of the rom
        // This will be in the same memory format as the rest of the memory.
        uint8_t * RDRAM;
        uint8_t * DMEM;
        uint8_t * IMEM;

        uint32_t * MI__INTR_REG;

        uint32_t * DPC__START_REG;
        uint32_t * DPC__END_REG;
        uint32_t * DPC__CURRENT_REG;
        uint32_t * DPC__STATUS_REG;
        uint32_t * DPC__CLOCK_REG;
        uint32_t * DPC__BUFBUSY_REG;
        uint32_t * DPC__PIPEBUSY_REG;
        uint32_t * DPC__TMEM_REG;

        uint32_t * VI__STATUS_REG;
        uint32_t * VI__ORIGIN_REG;
        uint32_t * VI__WIDTH_REG;
        uint32_t * VI__INTR_REG;
        uint32_t * VI__V_CURRENT_LINE_REG;
        uint32_t * VI__TIMING_REG;
        uint32_t * VI__V_SYNC_REG;
        uint32_t * VI__H_SYNC_REG;
        uint32_t * VI__LEAP_REG;
        uint32_t * VI__H_START_REG;
        uint32_t * VI__V_START_REG;
        uint32_t * VI__V_BURST_REG;
        uint32_t * VI__X_SCALE_REG;
        uint32_t * VI__Y_SCALE_REG;

        void(CALL *CheckInterrupts)(void);
#ifdef ANDROID
        void(CALL *SwapBuffers)(void);
#endif
    } GFX_INFO;

    //Get Function from DLL
    int32_t(CALL *InitiateGFX)(GFX_INFO Gfx_Info);
    _LoadFunction("InitiateGFX", InitiateGFX);
    if (InitiateGFX == NULL)
    {
        WriteTrace(TraceGFXPlugin, TraceDebug, "Failed to find InitiateGFX");
        return false;
    }

    GFX_INFO Info = { 0 };

    Info.MemoryBswaped = true;
#ifdef _WIN32
    Info.hWnd = Window ? Window->GetWindowHandle() : NULL;
    Info.hStatusBar = Window ? Window->GetStatusBar() : NULL;
#else
    Info.SwapBuffers = SwapBuffers;
    Info.hWnd = NULL;
    Info.hStatusBar = NULL;
#endif
    Info.CheckInterrupts = DummyCheckInterrupts;

    // We are initializing the plugin before any rom is loaded so we do not have any correct
    // parameters here.. it's just needed so we can config the DLL.
    if (System == NULL)
    {
        uint8_t Buffer[100];
        uint32_t Value = 0;

        Info.HEADER = Buffer;
        Info.RDRAM = Buffer;
        Info.DMEM = Buffer;
        Info.IMEM = Buffer;
        Info.MI__INTR_REG = &Value;
        Info.VI__STATUS_REG = &Value;
        Info.VI__ORIGIN_REG = &Value;
        Info.VI__WIDTH_REG = &Value;
        Info.VI__INTR_REG = &Value;
        Info.VI__V_CURRENT_LINE_REG = &Value;
        Info.VI__TIMING_REG = &Value;
        Info.VI__V_SYNC_REG = &Value;
        Info.VI__H_SYNC_REG = &Value;
        Info.VI__LEAP_REG = &Value;
        Info.VI__H_START_REG = &Value;
        Info.VI__V_START_REG = &Value;
        Info.VI__V_BURST_REG = &Value;
        Info.VI__X_SCALE_REG = &Value;
        Info.VI__Y_SCALE_REG = &Value;
    }
    // Send initialization information to the DLL
    else
    {
        Info.HEADER = g_Rom->GetRomAddress();
        Info.RDRAM = g_MMU->Rdram();
        Info.DMEM = g_MMU->Dmem();
        Info.IMEM = g_MMU->Imem();
        Info.MI__INTR_REG = &g_Reg->m_GfxIntrReg;
        Info.DPC__START_REG = &g_Reg->DPC_START_REG;
        Info.DPC__END_REG = &g_Reg->DPC_END_REG;
        Info.DPC__CURRENT_REG = &g_Reg->DPC_CURRENT_REG;
        Info.DPC__STATUS_REG = &g_Reg->DPC_STATUS_REG;
        Info.DPC__CLOCK_REG = &g_Reg->DPC_CLOCK_REG;
        Info.DPC__BUFBUSY_REG = &g_Reg->DPC_BUFBUSY_REG;
        Info.DPC__PIPEBUSY_REG = &g_Reg->DPC_PIPEBUSY_REG;
        Info.DPC__TMEM_REG = &g_Reg->DPC_TMEM_REG;
        Info.VI__STATUS_REG = &g_Reg->VI_STATUS_REG;
        Info.VI__ORIGIN_REG = &g_Reg->VI_ORIGIN_REG;
        Info.VI__WIDTH_REG = &g_Reg->VI_WIDTH_REG;
        Info.VI__INTR_REG = &g_Reg->VI_INTR_REG;
        Info.VI__V_CURRENT_LINE_REG = &g_Reg->VI_CURRENT_REG;
        Info.VI__TIMING_REG = &g_Reg->VI_TIMING_REG;
        Info.VI__V_SYNC_REG = &g_Reg->VI_V_SYNC_REG;
        Info.VI__H_SYNC_REG = &g_Reg->VI_H_SYNC_REG;
        Info.VI__LEAP_REG = &g_Reg->VI_LEAP_REG;
        Info.VI__H_START_REG = &g_Reg->VI_H_START_REG;
        Info.VI__V_START_REG = &g_Reg->VI_V_START_REG;
        Info.VI__V_BURST_REG = &g_Reg->VI_V_BURST_REG;
        Info.VI__X_SCALE_REG = &g_Reg->VI_X_SCALE_REG;
        Info.VI__Y_SCALE_REG = &g_Reg->VI_Y_SCALE_REG;
    }

    WriteTrace(TraceGFXPlugin, TraceDebug, "Calling InitiateGFX");
    m_Initialized = InitiateGFX(Info) != 0;

#ifdef _WIN32
    //jabo had a bug so I call CreateThread so his dllmain gets called again
    pjutil::DynLibCallDllMain();
#endif

    WriteTrace(TraceGFXPlugin, TraceDebug, "InitiateGFX done (res: %s)", m_Initialized ? "true" : "false");
    return m_Initialized;
}

void CGfxPlugin::UnloadPluginDetails(void)
{
    WriteTrace(TraceGFXPlugin, TraceDebug, "start");
    if (m_LibHandle != NULL)
    {
        pjutil::DynLibClose(m_LibHandle);
        m_LibHandle = NULL;
    }
    memset(&m_GFXDebug, 0, sizeof(m_GFXDebug));

    //	CaptureScreen        = NULL;
    ChangeWindow = NULL;
    GetDebugInfo = NULL;
    DrawScreen = NULL;
    DrawStatus = NULL;
    //	FrameBufferRead      = NULL;
    //	FrameBufferWrite     = NULL;
    InitiateDebugger = NULL;
    MoveScreen = NULL;
    ProcessDList = NULL;
    ProcessRDPList = NULL;
    ShowCFB = NULL;
    UpdateScreen = NULL;
    ViStatusChanged = NULL;
    ViWidthChanged = NULL;
    GetRomBrowserMenu = NULL;
    OnRomBrowserMenuItem = NULL;
    WriteTrace(TraceGFXPlugin, TraceDebug, "Done");
}

void CGfxPlugin::ProcessMenuItem(int32_t id)
{
    if (m_GFXDebug.ProcessMenuItem)
    {
        m_GFXDebug.ProcessMenuItem(id);
    }
}

#ifdef ANDROID
void CGfxPlugin::SwapBuffers(void)
{
    RenderWindow * render = g_Plugins ? g_Plugins->MainWindow() : NULL;
    WriteTrace(TraceGFXPlugin, TraceDebug, "Start (render: %p)",render);
    if (render != NULL)
    {
        render->SwapWindow();
    }
    WriteTrace(TraceGFXPlugin, TraceDebug, "Done");
}
#endif
