/**********************************************************************

   Audacity - A Digital Audio Editor
   Copyright 1999-2010 Audacity Team
   Michael Chinen

******************************************************************/


#include "DeviceManager.h"

#include <wx/log.h>
#include <thread>


#include "portaudio.h"
#ifdef __WXMSW__
#include "pa_win_wasapi.h"
#endif

#include "AudioIOBase.h"

#include "DeviceChange.h" // for HAVE_DEVICE_CHANGE

wxDEFINE_EVENT(EVT_RESCANNED_DEVICES, wxEvent);

DeviceManager DeviceManager::dm;

/// Gets the singleton instance
DeviceManager& DeviceManager::Instance()
{
   return dm;
}

const std::vector<Device> &DeviceManager::GetInputDevices()
{
   if (!m_inited)
      Init();
   return mInputDeviceSources;
}
const std::vector<Device> &DeviceManager::GetOutputDevices()
{
   if (!m_inited)
      Init();
   return mOutputDeviceSources;
}

Device* DeviceManager::GetDefaultDevice(int hostIndex, bool isInput)
{
   if (hostIndex < 0 || hostIndex >= Pa_GetHostApiCount()) {
      return nullptr;
   }

   const struct PaHostApiInfo *apiinfo = Pa_GetHostApiInfo(hostIndex);   // get info on API
   std::vector<Device>& devices = isInput ? mInputDeviceSources : mOutputDeviceSources;
   size_t i;
   int targetDevice = isInput ? apiinfo->defaultInputDevice : apiinfo->defaultOutputDevice;

   for (i = 0; i < devices.size(); i++)
   {
      if (devices[i].GetDeviceIndex() == targetDevice)
      {
         return &devices[i];
      }
   }

   wxLogDebug(wxT("GetDefaultDevice() no default device"));
   return nullptr;
}

Device* DeviceManager::GetDefaultOutputDevice(int hostIndex)
{
   return GetDefaultDevice(hostIndex, false);
}
Device* DeviceManager::GetDefaultInputDevice(int hostIndex)
{
   return GetDefaultDevice(hostIndex, true);
}

//--------------- Device Enumeration --------------------------


static void FillHostDeviceInfo(Device& device, const PaDeviceInfo& info, int deviceIndex, bool isInput)
{
   std::string hostapiName = Pa_GetHostApiInfo(info.hostApi)->name;
   std::string infoName = info.name;

   device.SetDeviceIndex(deviceIndex);
   device.SetHostIndex(info.hostApi);
   device.SetName(infoName);
   device.SetHostName(hostapiName);
   device.SetNumChannels(isInput ? info.maxInputChannels : info.maxOutputChannels);
   device.SetDeviceType(isInput ? Device::Type::Input : Device::Type::Output);
}

static bool IsInputDeviceAMapperDevice(const PaDeviceInfo& info)
{
   // For Windows only, portaudio returns the default mapper object
   // as the first index after a NEW hostApi index is detected (true for MME and DS)
   // this is a bit of a hack, but there's no other way to find out which device is a mapper,
   // I've looked at string comparisons, but if the system is in a different language this breaks.
#ifdef __WXMSW__
   static int lastHostApiTypeId = -1;
   int hostApiTypeId = Pa_GetHostApiInfo(info.hostApi)->type;
   if(hostApiTypeId != lastHostApiTypeId &&
      (hostApiTypeId == paMME || hostApiTypeId == paDirectSound)) {
      lastHostApiTypeId = hostApiTypeId;
      return true;
   }
#endif

   return false;
}

static void AddSources(int deviceIndex, int rate, std::vector<Device>& devices, bool isInput)
{
   Device device;
   const PaDeviceInfo *info = Pa_GetDeviceInfo(deviceIndex);

   // Only inputs have sources, so we call FillHostDeviceInfo with a 1 to indicate this
   FillHostDeviceInfo(device, *info, deviceIndex, true);

   devices.push_back(device);
}

namespace {
struct MyEvent : wxEvent {
   using wxEvent::wxEvent;
   wxEvent *Clone() const override { return new MyEvent{*this}; }
};
}

/// Gets a NEW list of devices by terminating and restarting portaudio
/// Assumes that DeviceManager is only used on the main thread.
void DeviceManager::Rescan()
{
   // get rid of the previous scan info
   this->mInputDeviceSources.clear();
   this->mOutputDeviceSources.clear();

   // if we are doing a second scan then restart portaudio to get NEW devices
   if (m_inited) {
      // check to see if there is a stream open - can happen if monitoring,
      // but otherwise Rescan() should not be available to the user.
      auto gAudioIO = AudioIOBase::Get();
      if (gAudioIO) {
         if (gAudioIO->IsMonitoring())
         {
            using namespace std::chrono;
            gAudioIO->StopStream();
            while (gAudioIO->IsBusy())
               std::this_thread::sleep_for(100ms);
         }
      }

      // restart portaudio - this updates the device list
      // FIXME: TRAP_ERR restarting PortAudio
      Pa_Terminate();
      Pa_Initialize();
   }

   // FIXME: TRAP_ERR PaErrorCode not handled in ReScan()
   int nDevices = Pa_GetDeviceCount();

   //The hierarchy for devices is Host/device/source.
   //Some newer systems aggregate this.
   //So we need to call port mixer for every device to get the sources
   for (int i = 0; i < nDevices; i++) {
      const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
      if (info->maxOutputChannels > 0) {
         AddSources(i, info->defaultSampleRate, mOutputDeviceSources, false);
      }

      if (info->maxInputChannels > 0) {
#ifdef __WXMSW__
#if !defined(EXPERIMENTAL_FULL_WASAPI)
         if (Pa_GetHostApiInfo(info->hostApi)->type != paWASAPI ||
             PaWasapi_IsLoopback(i) > 0)
#endif
#endif
         AddSources(i, info->defaultSampleRate, mInputDeviceSources, true);
      }
   }

   // If this was not an initial scan update each device toolbar.
   if ( m_inited ) {
      MyEvent e{ 0, EVT_RESCANNED_DEVICES };
      this->ProcessEvent( e );
   }

   m_inited = true;
   mRescanTime = std::chrono::steady_clock::now();
}


float DeviceManager::GetTimeSinceRescan() {
   auto now = std::chrono::steady_clock::now();
   auto dur = std::chrono::duration_cast<std::chrono::duration<float>>(now - mRescanTime);
   return dur.count();
}

//private constructor - Singleton.
DeviceManager::DeviceManager()
#if defined(EXPERIMENTAL_DEVICE_CHANGE_HANDLER)
#if defined(HAVE_DEVICE_CHANGE)
:  DeviceChangeHandler()
#endif
#endif
{
   m_inited = false;
   mRescanTime = std::chrono::steady_clock::now();
}

DeviceManager::~DeviceManager()
{

}

void DeviceManager::Init()
{
    Rescan();

#if defined(EXPERIMENTAL_DEVICE_CHANGE_HANDLER)
#if defined(HAVE_DEVICE_CHANGE)
   DeviceChangeHandler::Enable(true);
#endif
#endif
}

#if defined(EXPERIMENTAL_DEVICE_CHANGE_HANDLER)
#if defined(HAVE_DEVICE_CHANGE)
void DeviceManager::DeviceChangeNotification()
{
   Rescan();
   return;
}
#endif
#endif
