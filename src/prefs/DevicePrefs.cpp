/**********************************************************************

  Audacity: A Digital Audio Editor

  DevicePrefs.cpp

  Joshua Haberman
  Dominic Mazzoni
  James Crook

*******************************************************************//**

\class DevicePrefs
\brief A PrefsPanel used to select recording and playback devices and
other settings.

  Presents interface for user to select the recording device and
  playback device, from the list of choices that PortAudio
  makes available.

  Also lets user decide how many channels to record.

*//********************************************************************/


#include "DevicePrefs.h"

#include "RecordingPrefs.h"

#include <wx/defs.h>

#include <wx/choice.h>
#include <wx/intl.h>
#include <wx/log.h>
#include <wx/textctrl.h>

#include "portaudio.h"

// Tenacity libraries
#include <lib-audio-devices/AudioIOBase.h>
#include <lib-audio-devices/DeviceManager.h>
#include <lib-preferences/Prefs.h>
#include <lib-project-rate/QualitySettings.h>

#include "AudioIO.h"
#include "ProjectAudioManager.h"
#include "../shuttle/ShuttleGui.h"

enum {
   HostID = 10000,
   PlayID,
   RecordID,
   ChannelsID
};

BEGIN_EVENT_TABLE(DevicePrefs, PrefsPanel)
   EVT_CHOICE(HostID, DevicePrefs::OnHost)
   EVT_CHOICE(RecordID, DevicePrefs::OnDevice)
END_EVENT_TABLE()

DevicePrefs::DevicePrefs(wxWindow * parent, wxWindowID winid, TenacityProject* project)
:  PrefsPanel(parent, winid, XO("Devices")), mProject{project}
{
   Populate();
}

DevicePrefs::~DevicePrefs()
{
}


ComponentInterfaceSymbol DevicePrefs::GetSymbol()
{
   return DEVICE_PREFS_PLUGIN_SYMBOL;
}

TranslatableString DevicePrefs::GetDescription()
{
   return XO("Preferences for Device");
}

ManualPageID DevicePrefs::HelpPageName()
{
   return "Preferences#devices";
}

void DevicePrefs::Populate()
{
   // First any pre-processing for constructing the GUI.
   GetNamesAndLabels();

   // Get current setting for devices
   mPlayDevice = AudioIOPlaybackDevice.Read();
   mRecordDevice = AudioIORecordingDevice.Read();
   mRecordChannels = AudioIORecordChannels.Read();

   //------------------------- Main section --------------------
   // Now construct the GUI itself.
   // Use 'eIsCreatingFromPrefs' so that the GUI is
   // initialised with values from gPrefs.
   ShuttleGui S(this, eIsCreatingFromPrefs);
   PopulateOrExchange(S);
   // ----------------------- End of main section --------------

   wxCommandEvent e;
   OnHost(e);
}


/*
 * Get names of device hosts.
 */
void DevicePrefs::GetNamesAndLabels()
{
   // Gather list of hosts.  Only added hosts that have devices attached.
   // FIXME: TRAP_ERR PaErrorCode not handled in DevicePrefs GetNamesAndLabels()
   // With an error code won't add hosts, but won't report a problem either.
   int nDevices = Pa_GetDeviceCount();
   for (int i = 0; i < nDevices; i++) {
      const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
      if ((info!=NULL)&&(info->maxOutputChannels > 0 || info->maxInputChannels > 0)) {
         wxString name = wxSafeConvertMB2WX(Pa_GetHostApiInfo(info->hostApi)->name);
         if (!make_iterator_range(mHostNames)
            .contains( Verbatim( name ) )) {
            mHostNames.push_back( Verbatim( name ) );
            mHostLabels.push_back(name);
         }
      }
   }
}

void DevicePrefs::PopulateOrExchange(ShuttleGui & S)
{
   S.SetBorder(2);
   S.StartScroller();

   /* i18n-hint Software interface to audio devices */
   S.StartStatic(XC("Interface", "device"));
   {
      S.StartMultiColumn(2);
      {
         S.Id(HostID);
         mHost = S.TieChoice( XXO("&Host:"),
            {
               AudioIOHost,
               { ByColumns, mHostNames, mHostLabels }
            }
         );

         S.AddPrompt(XXO("Using:"));
         S.AddFixedText( Verbatim(wxSafeConvertMB2WX(Pa_GetVersionText() ) ) );
      }
      S.EndMultiColumn();
   }
   S.EndStatic();

   S.StartStatic(XO("Playback"));
   {
      S.StartMultiColumn(2);
      {
         S.Id(PlayID);
         mPlay = S.AddChoice(XXO("&Device:"),
                             {} );
      }
      S.EndMultiColumn();
   }
   S.EndStatic();

   // i18n-hint: modifier as in "Recording preferences", not progressive verb
   S.StartStatic(XC("Recording", "preference"));
   {
      S.StartMultiColumn(2);
      {
         S.Id(RecordID);
         mRecord = S.AddChoice(XXO("De&vice:"),
                               {} );

         S.Id(ChannelsID);
         mChannels = S.AddChoice(XXO("Cha&nnels:"),
                                 {} );
      }
      S.EndMultiColumn();
   }
   S.EndStatic();

   // These previously lived in recording preferences.
   // However they are liable to become device specific.
   // Buffering also affects playback, not just recording, so is a device characteristic.
   S.StartStatic( XO("Latency"));
   {
      S.StartThreeColumn();
      {
         S.NameSuffix(XO("milliseconds"))
          .TieNumericTextBox(XXO("&Buffer length:"),
                                 AudioIOLatencyDuration,
                                 9);
         S.TieChoice(XO(""), AudioIOLatencyUnit);

         S.NameSuffix(XO("milliseconds"))
          .TieNumericTextBox(XXO("&Latency compensation:"),
               AudioIOLatencyCorrection, 9);
         S.AddUnits(XO("milliseconds"));
      }
      S.EndThreeColumn();
   }
   S.EndStatic();
   S.EndScroller();

}

void DevicePrefs::OnHost(wxCommandEvent & e)
{
   // Bail if we have no hosts
   if (mHostNames.size() < 1)
      return;

   // Find the index for the host API selected
   int index = -1;
   auto apiName = mHostLabels[mHost->GetCurrentSelection()];
   int nHosts = Pa_GetHostApiCount();
   for (int i = 0; i < nHosts; ++i) {
      wxString name = wxSafeConvertMB2WX(Pa_GetHostApiInfo(i)->name);
      if (name == apiName) {
         index = i;
         break;
      }
   }
   // We should always find the host!
   if (index < 0) {
      wxLogDebug(wxT("DevicePrefs::OnHost(): API index not found"));
      return;
   }

   int nDevices = Pa_GetDeviceCount();

   // FIXME: TRAP_ERR PaErrorCode not handled.  nDevices can be negative number.
   if (nDevices == 0) {
      mHost->Clear();
      mHost->Append(_("No audio interfaces"), (void *) NULL);
      mHost->SetSelection(0);
   }

   const std::vector<Device> &inDevices  = DeviceManager::Instance().GetInputDevices();
   const std::vector<Device> &outDevices = DeviceManager::Instance().GetOutputDevices();

   wxArrayString playnames;
   wxArrayString recordnames;
   size_t i;
   int devindex;  /* temp variable to hold the numeric ID of each device in turn */
   std::string device;
   std::string recDevice;

   recDevice = mRecordDevice;

   mRecord->Clear();
   for (i = 0; i < inDevices.size(); i++) {
      if (index == inDevices[i].GetHostIndex()) {
         device   = inDevices[i].GetName();
         devindex = mRecord->Append(device);
         // We need to const cast here because SetClientData is a wx function
         // It is okay because the original variable is non-const.
         mRecord->SetClientData(devindex, const_cast<Device*>(&inDevices[i]));
         if (device == recDevice) {  /* if this is the default device, select it */
            mRecord->SetSelection(devindex);
         }
      }
   }

   mPlay->Clear();
   for (i = 0; i < outDevices.size(); i++) {
      if (index == outDevices[i].GetHostIndex()) {
         device   = outDevices[i].GetName();
         devindex = mPlay->Append(device);
         mPlay->SetClientData(devindex, const_cast<Device*>(&outDevices[i]));
         if (device == mPlayDevice) {  /* if this is the default device, select it */
            mPlay->SetSelection(devindex);
         }
      }
   }

   /* deal with not having any devices at all */
   if (mPlay->GetCount() == 0) {
      playnames.push_back(_("No devices found"));
      mPlay->Append(playnames[0], (void *) NULL);
      mPlay->SetSelection(0);
   }
   if (mRecord->GetCount() == 0) {
      recordnames.push_back(_("No devices found"));
      mRecord->Append(recordnames[0], (void *) NULL);
      mRecord->SetSelection(0);
   }

   /* what if we have no device selected? we should choose the default on
    * this API, as defined by PortAudio. We then fall back to using 0 only if
    * that fails */
   if (mPlay->GetCount() && mPlay->GetSelection() == wxNOT_FOUND) {
      Device* defaultDevice = DeviceManager::Instance().GetDefaultOutputDevice(index);
      if (defaultDevice)
      {
         mPlay->SetStringSelection(defaultDevice->GetName());
      }

      if (mPlay->GetSelection() == wxNOT_FOUND) {
         mPlay->SetSelection(0);
      }
   }

   if (mRecord->GetCount() && mRecord->GetSelection() == wxNOT_FOUND) {
      Device* defaultDevice = DeviceManager::Instance().GetDefaultInputDevice(index);
      if (defaultDevice)
      {
         mRecord->SetStringSelection(defaultDevice->GetName());
      }

      if (mPlay->GetSelection() == wxNOT_FOUND) {
         mPlay->SetSelection(0);
      }
   }

   ShuttleGui::SetMinSize(mPlay, mPlay->GetStrings());
   ShuttleGui::SetMinSize(mRecord, mRecord->GetStrings());
   OnDevice(e);
}

void DevicePrefs::OnDevice(wxCommandEvent & WXUNUSED(event))
{
   int ndx = mRecord->GetCurrentSelection();
   if (ndx == wxNOT_FOUND) {
      ndx = 0;
   }

   int sel = mChannels->GetSelection();
   int cnt = 0;

   Device* inMap = (Device*) mRecord->GetClientData(ndx);
   if (inMap != NULL) {
      cnt = inMap->GetNumChannels();
   }

   if (sel != wxNOT_FOUND) {
      mRecordChannels = sel + 1;
   }

   mChannels->Clear();

   // Mimic old behavior
   if (cnt <= 0) {
      cnt = 16;
   }

   // Place an artificial limit on the number of channels to prevent an
   // outrageous number.  I don't know if this is really necessary, but
   // it doesn't hurt.
   if (cnt > 256) {
      cnt = 256;
   }

   wxArrayStringEx channelnames;

   // Channel counts, mono, stereo etc...
   for (int i = 0; i < cnt; i++) {
      wxString name;

      if (i == 0) {
         name = _("1 (Mono)");
      }
      else if (i == 1) {
         name = _("2 (Stereo)");
      }
      else {
         name = wxString::Format(wxT("%d"), i + 1);
      }

      channelnames.push_back(name);
      int index = mChannels->Append(name);
      if (i == mRecordChannels - 1) {
         mChannels->SetSelection(index);
      }
   }

   if (mChannels->GetCount() && mChannels->GetCurrentSelection() == wxNOT_FOUND) {
      mChannels->SetSelection(0);
   }

   ShuttleGui::SetMinSize(mChannels, channelnames);
   Layout();
}

bool DevicePrefs::Commit()
{
   ShuttleGui S(this, eIsSavingToPrefs);
   PopulateOrExchange(S);
   Device* device = nullptr;

   if (mPlay->GetCount() > 0) {
      device = (Device*) mPlay->GetClientData(
            mPlay->GetSelection());
   }
   if (device)
      AudioIOPlaybackDevice.Write(device->GetName());

   device = nullptr;
   if (mRecord->GetCount() > 0) {
      device = (Device*) mRecord->GetClientData(mRecord->GetSelection());
   }
   if (device) {
      AudioIORecordingDevice.Write(device->GetName());
      AudioIORecordChannels.Write(mChannels->GetSelection() + 1);
   }

   auto* audioIO = AudioIO::Get();
   bool monitoring = audioIO->IsMonitoring();
   if (monitoring)
   {
      audioIO->StopStream();
   }

   double latency = AudioIOLatencyDuration.Read();
   bool isMilliseconds = AudioIOLatencyUnit.Read() == "milliseconds";

   // If in milliseconds, convert the latency to samples
   if (isMilliseconds)
   {
      latency *= QualitySettings::DefaultSampleRate.Read() / 1000;
   }

   // The minimum latency setting is limited to either 32 samples or an
   // equivalent time in milliseconds at the current sample rate. If the
   // preference is below this setting, automatically set it to either 32
   // samples or the equivalent time in milliseconds.
   //
   // Why limit the preference to 32 samples? No reason, except it's a pretty
   // small value already :)
   if (static_cast<int>(latency) < 32)
   {
      latency = 32.0;

      if (isMilliseconds)
      {
         latency /= QualitySettings::DefaultSampleRate.Read() / 1000;
      }

      AudioIOLatencyDuration.Write(latency);
   }

   AudioIO::Get()->UpdateBuffers();

   if (monitoring)
   {
      audioIO->StartMonitoring(DefaultPlayOptions(*mProject));
   }

   return true;
}

namespace{
PrefsPanel::Registration sAttachment{ "Device",
   [](wxWindow *parent, wxWindowID winid, TenacityProject* project)
   {
      wxASSERT(parent); // to justify safenew
      return safenew DevicePrefs(parent, winid, project);
   }
};
}
