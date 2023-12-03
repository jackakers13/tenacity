/**********************************************************************

  Audacity: A Digital Audio Editor

  DeviceManager.h

  Created by Michael Chinen (mchinen) on 2/12/11
  Audacity(R) is copyright (c) 1999-2011 Audacity Team.
  License: GPL v2.  See License.txt.

******************************************************************//**

\class DeviceManager
\brief A singleton that manages the audio devices known to Audacity

*//*******************************************************************/

#ifndef __AUDACITY_DEVICEMANAGER__
#define __AUDACITY_DEVICEMANAGER__

#include <chrono>
#include <vector>

#include <wx/event.h> // to declare a custom event type

#if defined(EXPERIMENTAL_DEVICE_CHANGE_HANDLER)
#include "DeviceChange.h"
#endif

#include "Device.h"

// Event sent to the application
wxDECLARE_EXPORTED_EVENT(AUDIO_DEVICES_API,
                         EVT_RESCANNED_DEVICES, wxEvent);

class AUDIO_DEVICES_API DeviceManager final
#if defined(EXPERIMENTAL_DEVICE_CHANGE_HANDLER) && defined(HAVE_DEVICE_CHANGE)
: public DeviceChangeHandler
#else
: public wxEvtHandler
#endif
{
 public:
   /// Gets the singleton instance
   static DeviceManager& Instance();

   /// Gets a NEW list of devices by terminating and restarting portaudio
   /// Assumes that DeviceManager is only used on the main thread.
   void Rescan();

   // Time since devices scanned in seconds.
   float GetTimeSinceRescan();

   Device* GetDefaultOutputDevice(int hostIndex);
   Device* GetDefaultInputDevice(int hostIndex);

   const std::vector<Device> &GetInputDevices();
   const std::vector<Device> &GetOutputDevices();

#if defined(EXPERIMENTAL_DEVICE_CHANGE_HANDLER)
#if defined(HAVE_DEVICE_CHANGE)
   // DeviceChangeHandler implementation
   void DeviceChangeNotification();
#endif
#endif

private:
   std::chrono::time_point<std::chrono::steady_clock> mRescanTime;

 protected:
   //private constructor - Singleton.
   DeviceManager();
   ~DeviceManager();
   /// Does an initial scan.
   /// Called by GetInputDeviceMaps and GetOutputDeviceMaps when needed.
   void Init();

   Device* GetDefaultDevice(int hostIndex, bool isInput);

   bool m_inited;

   std::vector<Device> mInputDeviceSources;
   std::vector<Device> mOutputDeviceSources;

   static DeviceManager dm;
};

#endif

