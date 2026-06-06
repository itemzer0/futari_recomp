#include <rex/rex.h>
#include <cstdio>

// ---------------------------------------------------------------------------
// Xbox 360 kernel / XAM stubs for Mushihimesama Futari Ver. 1.5
//
// Full import list discovered from generated/ppc_func_mapping.cpp.
// All stubs return 0 by default. Implement the ones needed for the game
// to progress, in order of importance:
//   1. Kernel primitives (Ke*, Nt*, Rtl*)  — needed for boot
//   2. File I/O (NtCreateFile, NtReadFile, etc.) — needed for asset load
//   3. Audio (XAudio*, XMA*)               — needed for sound
//   4. Video (Vd*)                          — needed for rendering
//   5. XAM UI / XboxLive                   — no-ops for offline play
// ---------------------------------------------------------------------------

#define REX_STUB(name) \
    REX_IMPORT(name) { ctx.r3.u32 = 0; }

// --- CRT helpers ---
REX_IMPORT(__imp___vsnprintf) {
    // args: r3=buf, r4=count, r5=fmt, r6=args
    // TODO: implement properly
    ctx.r3.u32 = 0;
}
REX_STUB(__imp__sprintf)

// --- Debug ---
REX_IMPORT(__imp__DbgPrint) {
    // r3 = format string ptr (guest addr)
    uint8_t* mem = RexGetMemoryBase();
    const char* fmt = reinterpret_cast<const char*>(mem + ctx.r3.u32);
    printf("[DbgPrint] %s", fmt);
    ctx.r3.u32 = 0;
}
REX_STUB(__imp__KeBugCheck)
REX_STUB(__imp__KeBugCheckEx)
REX_STUB(__imp__RtlRaiseException)
REX_STUB(__imp__RtlCaptureContext)
REX_STUB(__imp__RtlUnwind)
REX_STUB(__imp____C_specific_handler)

// --- Kernel thread / event primitives ---
REX_STUB(__imp__ExCreateThread)
REX_STUB(__imp__ExTerminateThread)
REX_STUB(__imp__ExRegisterTitleTerminateNotification)
REX_STUB(__imp__KeDelayExecutionThread)
REX_STUB(__imp__KeEnterCriticalRegion)
REX_STUB(__imp__KeLeaveCriticalRegion)
REX_STUB(__imp__KeGetCurrentProcessType)
REX_STUB(__imp__KeQueryBasePriorityThread)
REX_STUB(__imp__KeSetBasePriorityThread)
REX_STUB(__imp__KeSetAffinityThread)
REX_STUB(__imp__KeAcquireSpinLockAtRaisedIrql)
REX_STUB(__imp__KeReleaseSpinLockFromRaisedIrql)
REX_STUB(__imp__KfAcquireSpinLock)
REX_STUB(__imp__KfReleaseSpinLock)
REX_STUB(__imp__KeLockL2)
REX_STUB(__imp__KeUnlockL2)
REX_STUB(__imp__KeInitializeDpc)
REX_STUB(__imp__KiApcNormalRoutineNop)
REX_STUB(__imp__KeSetEvent)
REX_STUB(__imp__KeResetEvent)
REX_STUB(__imp__KeWaitForSingleObject)
REX_STUB(__imp__KeWaitForMultipleObjects)
REX_STUB(__imp__KeTlsAlloc)
REX_STUB(__imp__KeTlsFree)
REX_STUB(__imp__KeTlsGetValue)
REX_STUB(__imp__KeTlsSetValue)

// --- Nt* (syscall layer) ---
REX_STUB(__imp__NtAllocateVirtualMemory)
REX_STUB(__imp__NtFreeVirtualMemory)
REX_STUB(__imp__NtQueryVirtualMemory)
REX_STUB(__imp__NtQueryAddressProtect)
// TODO: implement file I/O for asset loading
REX_STUB(__imp__NtCreateFile)
REX_STUB(__imp__NtOpenFile)
REX_STUB(__imp__NtReadFile)
REX_STUB(__imp__NtReadFileScatter)
REX_STUB(__imp__NtWriteFile)
REX_STUB(__imp__NtFlushBuffersFile)
REX_STUB(__imp__NtQueryInformationFile)
REX_STUB(__imp__NtSetInformationFile)
REX_STUB(__imp__NtQueryDirectoryFile)
REX_STUB(__imp__NtQueryFullAttributesFile)
REX_STUB(__imp__NtQueryVolumeInformationFile)
REX_STUB(__imp__NtCreateEvent)
REX_STUB(__imp__NtClearEvent)
REX_STUB(__imp__NtSetEvent)
REX_STUB(__imp__NtCreateSemaphore)
REX_STUB(__imp__NtReleaseSemaphore)
REX_STUB(__imp__NtWaitForSingleObjectEx)
REX_STUB(__imp__NtWaitForMultipleObjectsEx)
REX_STUB(__imp__NtClose)
REX_STUB(__imp__NtDuplicateObject)
REX_STUB(__imp__NtResumeThread)

// --- Object manager ---
REX_STUB(__imp__ObReferenceObjectByHandle)
REX_STUB(__imp__ObDereferenceObject)
REX_STUB(__imp__ObCreateSymbolicLink)
REX_STUB(__imp__ObDeleteSymbolicLink)

// --- Rtl* ---
REX_STUB(__imp__RtlInitAnsiString)
REX_STUB(__imp__RtlInitializeCriticalSection)
REX_STUB(__imp__RtlInitializeCriticalSectionAndSpinCount)
REX_STUB(__imp__RtlEnterCriticalSection)
REX_STUB(__imp__RtlLeaveCriticalSection)
REX_STUB(__imp__RtlTryEnterCriticalSection)
REX_STUB(__imp__RtlCompareMemoryUlong)
REX_STUB(__imp__RtlFillMemoryUlong)
REX_STUB(__imp__RtlMultiByteToUnicodeN)
REX_STUB(__imp__RtlUnicodeToMultiByteN)
REX_STUB(__imp__RtlNtStatusToDosError)
REX_STUB(__imp__RtlImageXexHeaderField)
REX_STUB(__imp__RtlTimeFieldsToTime)
REX_STUB(__imp__RtlTimeToTimeFields)

// --- Mm* (physical memory) ---
REX_STUB(__imp__MmAllocatePhysicalMemoryEx)
REX_STUB(__imp__MmFreePhysicalMemory)
REX_STUB(__imp__MmGetPhysicalAddress)
REX_STUB(__imp__MmMapIoSpace)
REX_STUB(__imp__MmQueryAddressProtect)

// --- Hal / system ---
REX_STUB(__imp__HalReturnToFirmware)
REX_STUB(__imp__ExGetXConfigSetting)
REX_STUB(__imp__KeQueryPerformanceFrequency)
REX_STUB(__imp__KeQuerySystemTime)
REX_STUB(__imp__XGetLanguage)
REX_STUB(__imp__XGetAVPack)
REX_STUB(__imp__XGetVideoMode)

// --- Xex module loading ---
REX_STUB(__imp__XexLoadImage)
REX_STUB(__imp__XexUnloadImage)
REX_STUB(__imp__XexGetModuleHandle)
REX_STUB(__imp__XexGetProcedureAddress)
REX_STUB(__imp__XexCheckExecutablePrivilege)

// --- XMsg (cross-process messaging) ---
REX_STUB(__imp__XMsgInProcessCall)
REX_STUB(__imp__XMsgStartIORequest)
REX_STUB(__imp__XMsgCancelIORequest)

// --- Video display (Vd*) ---
// TODO: implement rendering backend
REX_STUB(__imp__VdInitializeEngines)
REX_STUB(__imp__VdShutdownEngines)
REX_STUB(__imp__VdInitializeRingBuffer)
REX_STUB(__imp__VdGetSystemCommandBuffer)
REX_STUB(__imp__VdSetSystemCommandBufferGpuIdentifierAddress)
REX_STUB(__imp__VdInitializeScalerCommandBuffer)
REX_STUB(__imp__VdEnableRingBufferRPtrWriteBack)
REX_STUB(__imp__VdCallGraphicsNotificationRoutines)
REX_STUB(__imp__VdSetGraphicsInterruptCallback)
REX_STUB(__imp__VdSetDisplayMode)
REX_STUB(__imp__VdQueryVideoMode)
REX_STUB(__imp__VdQueryVideoFlags)
REX_STUB(__imp__VdGetCurrentDisplayInformation)
REX_STUB(__imp__VdGetCurrentDisplayGamma)
REX_STUB(__imp__VdEnableDisableClockGating)
REX_STUB(__imp__VdIsHSIOTrainingSucceeded)
REX_STUB(__imp__VdPersistDisplay)
REX_STUB(__imp__VdRetrainEDRAM)
REX_STUB(__imp__VdRetrainEDRAMWorker)
REX_STUB(__imp__VdSwap)

// --- Audio (XAudio*, XMA*) ---
// TODO: implement audio backend (XMA2 → standard PCM decode)
REX_STUB(__imp__XAudioGetSpeakerConfig)
REX_STUB(__imp__XAudioGetVoiceCategoryVolume)
REX_STUB(__imp__XAudioRegisterRenderDriverClient)
REX_STUB(__imp__XAudioUnregisterRenderDriverClient)
REX_STUB(__imp__XAudioSubmitRenderDriverFrame)
REX_STUB(__imp__XMACreateContext)
REX_STUB(__imp__XMAReleaseContext)

// --- Network (offline no-ops) ---
REX_STUB(__imp__NetDll_WSAStartup)
REX_STUB(__imp__NetDll_WSACleanup)
REX_STUB(__imp__NetDll_WSACreateEvent)
REX_STUB(__imp__NetDll_WSACloseEvent)
REX_STUB(__imp__NetDll_WSAResetEvent)
REX_STUB(__imp__NetDll_WSAWaitForMultipleEvents)
REX_STUB(__imp__NetDll_XNetStartup)
REX_STUB(__imp__NetDll_XNetCleanup)
REX_STUB(__imp__NetDll_XNetGetEthernetLinkStatus)
REX_STUB(__imp__NetDll_XNetQosServiceLookup)
REX_STUB(__imp__NetDll_XNetQosRelease)
REX_STUB(__imp__XNetLogonGetTitleID)

// --- XAM (UI, content, input) ---
REX_STUB(__imp__XamAlloc)
REX_STUB(__imp__XamFree)
REX_STUB(__imp__XamGetExecutionId)
REX_STUB(__imp__XamGetSystemVersion)
REX_STUB(__imp__XamLoaderTerminateTitle)
REX_STUB(__imp__XamBackgroundDownloadSetMode)
// Input — TODO: wire to real gamepad/keyboard input
REX_STUB(__imp__XamInputGetState)
REX_STUB(__imp__XamInputGetKeystrokeEx)
// Content / storage
REX_STUB(__imp__XamContentCreateEx)
REX_STUB(__imp__XamContentClose)
REX_STUB(__imp__XamContentCreateEnumerator)
REX_STUB(__imp__XamContentGetCreator)
REX_STUB(__imp__XamContentGetDeviceData)
REX_STUB(__imp__XamShowDeviceSelectorUI)
REX_STUB(__imp__XamEnumerate)
// User / profile
REX_STUB(__imp__XamUserGetSigninState)
REX_STUB(__imp__XamUserGetXUID)
REX_STUB(__imp__XamUserGetName)
REX_STUB(__imp__XamUserCheckPrivilege)
REX_STUB(__imp__XamUserAreUsersFriends)
REX_STUB(__imp__XamUserReadProfileSettings)
REX_STUB(__imp__XamUserCreateStatsEnumerator)
// Notifications / UI
REX_STUB(__imp__XamNotifyCreateListener)
REX_STUB(__imp__XamShowSigninUI)
REX_STUB(__imp__XamShowGamerCardUIForXUID)
REX_STUB(__imp__XamShowMarketplaceDownloadItemsUI)
REX_STUB(__imp__XamShowMessageBoxUIEx)
// Sessions
REX_STUB(__imp__XamSessionCreateHandle)
REX_STUB(__imp__XamSessionRefObjByHandle)
// Notifications
REX_STUB(__imp__XNotifyGetNext)
REX_STUB(__imp__XNotifyPositionUI)


void RegisterStubs() {
    // ReXGlue auto-discovers stubs via linker whole-archive; no body needed.
}
