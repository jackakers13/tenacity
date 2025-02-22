/**********************************************************************

  Audacity: A Digital Audio Editor

  Effect.h

  Dominic Mazzoni
  Vaughan Johnson

**********************************************************************/

#ifndef __AUDACITY_EFFECT__
#define __AUDACITY_EFFECT__



#include <functional>
#include <set>

#include <wx/defs.h>

class wxButton;
class wxCheckBox;
class wxChoice;
class wxListBox;
class wxWindow;

// Tenacity libraries
#include <lib-math/SampleCount.h>
#include <lib-screen-geometry/SelectedRegion.h>

#include "ConfigInterface.h"
#include "EffectHostInterface.h" // to inherit
#include "EffectInterface.h" // to inherit
#include "PluginInterface.h"

#include "Track.h"

#include "../widgets/wxPanelWrapper.h" // to inherit
#include <wx/windowptr.h>

class wxArrayString;
class ShuttleGui;
class AudacityCommand;

#define BUILTIN_EFFECT_PREFIX wxT("Built-in Effect: ")

namespace BasicUI { class ProgressDialog; }

class TenacityProject;
class LabelTrack;
class NotifyingSelectedRegion;
class SelectedRegion;
class Track;
class TrackList;
class WaveTrackFactory;
class WaveTrack;

using FloatBuffers = ArraysOf<float>;

/* i18n-hint: "Nyquist" is an embedded interpreted programming language in
 Audacity, named in honor of the Swedish-American Harry Nyquist (or Nyqvist).
 In the translations of this and other strings, you may transliterate the
 name into another alphabet.  */
#define NYQUISTEFFECTS_FAMILY ( EffectFamilySymbol{ XO("Nyquist") } )

#define NYQUIST_WORKER_ID wxT("Nyquist Worker")

// TODO:  Apr-06-2015
// TODO:  Much more cleanup of old methods and variables is needed, but
// TODO:  can't be done until after all effects are using the NEW API.

//! An Effect object is at once host and client:  it is self-hosting.
class TENACITY_DLL_API Effect /* not final */ : public wxEvtHandler,
   public EffectUIClientInterface,
   public EffectUIHostInterface
{
 //
 // public methods
 //
 // Used by the outside program to determine properties of an effect and
 // apply the effect to one or more tracks.
 //
 public:
   // The constructor is called once by each subclass at the beginning of the program.
   // Avoid allocating memory or doing time-consuming processing here.
   Effect();
   virtual ~Effect();

   // Type of a registered function that, if it returns true,
   // causes ShowInterface to return early without making any dialog
   using VetoDialogHook = bool (*) ( wxDialog* );
   static VetoDialogHook SetVetoDialogHook( VetoDialogHook hook );

   // ComponentInterface implementation

   PluginPath GetPath() override;

   ComponentInterfaceSymbol GetSymbol() override;

   VendorSymbol GetVendor() override;
   wxString GetVersion() override;
   TranslatableString GetDescription() override;

   // EffectDefinitionInterface implementation

   EffectType GetType() override;
   EffectFamilySymbol GetFamily() override;
   bool IsInteractive() override;
   bool IsDefault() override;
   bool SupportsRealtime() override;
   bool SupportsAutomation() override;

   bool GetAutomationParameters(CommandParameters & parms) override;
   bool SetAutomationParameters(CommandParameters & parms) override;

   bool LoadUserPreset(const RegistryPath & name) override;
   bool SaveUserPreset(const RegistryPath & name) override;

   RegistryPaths GetFactoryPresets() override;
   bool LoadFactoryPreset(int id) override;
   bool LoadFactoryDefaults() override;

   // EffectProcessor implementation

   bool SetHost(EffectHostInterface *host) override;
   
   unsigned GetAudioInCount() override;
   unsigned GetAudioOutCount() override;

   int GetMidiInCount() override;
   int GetMidiOutCount() override;

   sampleCount GetLatency() override;
   size_t GetTailSize() override;

   void SetSampleRate(double rate) override;
   size_t SetBlockSize(size_t maxBlockSize) override;
   size_t GetBlockSize() const override;

   bool ProcessInitialize(sampleCount totalLen, ChannelNames chanMap = NULL) override;
   bool ProcessFinalize() override;
   size_t ProcessBlock( const float *const *inBlock, float *const *outBlock,
      size_t blockLen) override;

   bool RealtimeInitialize() override;
   bool RealtimeAddProcessor(unsigned numChannels, float sampleRate) override;
   bool RealtimeFinalize() noexcept override;
   bool RealtimeSuspend() override;
   bool RealtimeResume() noexcept override;
   bool RealtimeProcessStart() override;
   size_t RealtimeProcess(int group, const float *const *inbuf,
      float *const *outbuf, size_t numSamples) override;
   bool RealtimeProcessEnd() noexcept override;

   int ShowClientInterface(
      wxWindow &parent, wxDialog &dialog, bool forceModal = false) override;

   // EffectUIClientInterface implementation

   bool PopulateUI(ShuttleGui &S) final;
   bool IsGraphicalUI() override;
   bool ValidateUI() override;
   bool HideUI() override;
   bool CloseUI() override;

   bool CanExportPresets() override;
   void ExportPresets() override;
   void ImportPresets() override;

   bool HasOptions() override;
   void ShowOptions() override;

   // EffectHostInterface implementation

   EffectDefinitionInterface &GetDefinition() override;
   double GetDuration() override;
   NumericFormatSymbol GetDurationFormat() override;
   virtual NumericFormatSymbol GetSelectionFormat() /* not override? */; // time format in Selection toolbar
   void SetDuration(double duration) override;

   RegistryPath GetUserPresetsGroup(const RegistryPath & name) override;
   RegistryPath GetCurrentSettingsGroup() override;
   RegistryPath GetFactoryDefaultsGroup() override;

   // EffectUIHostInterface implementation

   int ShowHostInterface( wxWindow &parent,
      const EffectDialogFactory &factory, bool forceModal = false) override;
   // The Effect class fully implements the Preview method for you.
   // Only override it if you need to do preprocessing or cleanup.
   void Preview(bool dryOnly) override;
   bool GetAutomationParametersAsString(wxString & parms) override;
   bool SetAutomationParametersFromString(const wxString & parms) override;
   bool IsBatchProcessing() override;
   void SetBatchProcessing(bool start) override;
   bool DoEffect( double projectRate, TrackList *list,
      WaveTrackFactory *factory, NotifyingSelectedRegion &selectedRegion,
      unsigned flags,
      // Prompt the user for input only if these arguments are both not null.
      wxWindow *pParent,
      const EffectDialogFactory &dialogFactory) override;
   bool Startup(EffectUIClientInterface *client) override;
   bool TransferDataToWindow() override;
   bool TransferDataFromWindow() override;

   // Effect implementation

   unsigned TestUIFlags(unsigned mask);

   void SetPresetParameters( const wxArrayString * Names, const wxArrayString * Values ) {
      if( Names ) mPresetNames = *Names;
      if( Values ) mPresetValues = *Values;
   }

   bool Delegate( Effect &delegate,
      wxWindow &parent, const EffectDialogFactory &factory );

   // Display a message box, using effect's (translated) name as the prefix
   // for the title.
   enum : long { DefaultMessageBoxStyle = wxOK | wxCENTRE };
   int MessageBox(const TranslatableString& message,
                  long style = DefaultMessageBoxStyle,
                  const TranslatableString& titleStr = {});

   static void IncEffectCounter(){ nEffectsDone++;};

 protected:
   bool EnableApply(bool enable = true);
   bool EnablePreview(bool enable = true);

 public:
   // NEW virtuals
   virtual bool Startup();

//
// protected virtual methods
//
// Each subclass of Effect overrides one or more of these methods to
// do its processing.
//
protected:

   // Called once each time an effect is called.  Perform any initialization;
   // make sure that the effect can be performed on the selected tracks and
   // return false otherwise
   virtual bool Init();

   // Check whether effect should be skipped
   // Typically this is only useful in automation, for example
   // detecting that zero noise reduction is to be done,
   // or that normalisation is being done without Dc bias shift
   // or amplitude modification
   virtual bool CheckWhetherSkipEffect() { return false; }

   // Actually do the effect here.
   /*! If Process() is not overridden, it uses ProcessInitialize(),
    ProcessBlock(), and ProcessFinalize() methods of EffectProcessor,
    and also GetLatency() to determine how many leading output samples to
    discard and how many extra samples to produce. */
   virtual bool Process();
   virtual bool ProcessPass();
   virtual bool InitPass1();
   virtual bool InitPass2();

   // clean up any temporary memory, needed only per invocation of the
   // effect, after either successful or failed or exception-aborted processing.
   // Invoked inside a "finally" block so it must be no-throw.
   virtual void End();

   // Most effects just use the previewLength, but time-stretching/compressing
   // effects need to use a different input length, so override this method.
   virtual double CalcPreviewInputLength(double previewLength);

   virtual void PopulateOrExchange(ShuttleGui & S);

   // No more virtuals!

   // The Progress methods all return true if the user has cancelled;
   // you should exit immediately if this happens (cleaning up memory
   // is okay, but don't try to undo).

   // Pass a fraction between 0.0 and 1.0
   bool TotalProgress(double frac, const TranslatableString & = {});

   // Pass a fraction between 0.0 and 1.0, for the current track
   // (when doing one track at a time)
   bool TrackProgress(int whichTrack, double frac, const TranslatableString & = {});

   // Pass a fraction between 0.0 and 1.0, for the current track group
   // (when doing stereo groups at a time)
   bool TrackGroupProgress(int whichGroup, double frac, const TranslatableString & = {});

   int GetNumWaveTracks() { return mNumTracks; }
   int GetNumWaveGroups() { return mNumGroups; }

   // Calculates the start time and length in samples for one or two channels
   void GetBounds(
      const WaveTrack &track, const WaveTrack *pRight,
      sampleCount *start, sampleCount *len);

   // Previewing linear effect can be optimised by pre-mixing. However this
   // should not be used for non-linear effects such as dynamic processors
   // To allow pre-mixing before Preview, set linearEffectFlag to true.
   void SetLinearEffectFlag(bool linearEffectFlag);

   // Most effects only need to preview a short selection. However some
   // (such as fade effects) need to know the full selection length.
   void SetPreviewFullSelectionFlag(bool previewDurationFlag);

   // Use this if the effect needs to know if it is previewing
   bool IsPreviewing() { return mIsPreview; }

   // Most effects only require selected tracks to be copied for Preview.
   // If IncludeNotSelectedPreviewTracks(true), then non-linear effects have
   // preview copies of all wave tracks.
   void IncludeNotSelectedPreviewTracks(bool includeNotSelected);

   // Use this method to copy the input tracks to mOutputTracks, if
   // doing the processing on them, and replacing the originals only on success (and not cancel).
   // If not all sync-locked selected, then only selected wave tracks.
   void CopyInputTracks(bool allSyncLockSelected = false);

   // A global counter of all the successful Effect invocations.
   static int nEffectsDone;

   // For the use of analyzers, which don't need to make output wave tracks,
   // but may need to add label tracks.
   class TENACITY_DLL_API AddedAnalysisTrack {
      friend Effect;
      AddedAnalysisTrack(Effect *pEffect, const wxString &name);
      AddedAnalysisTrack(const AddedAnalysisTrack&) = delete;

   public:

      AddedAnalysisTrack() {}

      // So you can have a vector of them
      AddedAnalysisTrack(AddedAnalysisTrack &&that);

      LabelTrack *get() const { return mpTrack; }

      // Call this to indicate successful completion of the analyzer.
      void Commit();

      // Destructor undoes the addition of the analysis track if not committed.
      ~AddedAnalysisTrack();

   private:
      Effect *mpEffect{};
      LabelTrack *mpTrack{};
   };

   // Set name to given value if that is not empty, else use default name
   std::shared_ptr<AddedAnalysisTrack> AddAnalysisTrack(const wxString &name = wxString());

   // For the use of analyzers, which don't need to make output wave tracks,
   // but may need to modify label tracks.
   class TENACITY_DLL_API ModifiedAnalysisTrack {
      friend Effect;
      ModifiedAnalysisTrack
         (Effect *pEffect, const LabelTrack *pOrigTrack, const wxString &name);
      ModifiedAnalysisTrack(const ModifiedAnalysisTrack&) = delete;

   public:

      ModifiedAnalysisTrack();

      // So you can have a vector of them
      ModifiedAnalysisTrack(ModifiedAnalysisTrack &&that);

      LabelTrack *get() const { return mpTrack; }

      // Call this to indicate successful completion of the analyzer.
      void Commit();

      // Destructor undoes the modification of the analysis track if not committed.
      ~ModifiedAnalysisTrack();

   private:
      Effect *mpEffect{};
      LabelTrack *mpTrack{};
      std::shared_ptr<Track> mpOrigTrack{};
   };

   // Set name to given value if that is not empty, else use default name
   ModifiedAnalysisTrack ModifyAnalysisTrack
      (const LabelTrack *pOrigTrack, const wxString &name = wxString());

   // If bGoodResult, replace mWaveTracks tracks in mTracks with successfully processed
   // mOutputTracks copies, get rid of old mWaveTracks, and set mWaveTracks to mOutputTracks.
   // Else clear and DELETE mOutputTracks copies.
   void ReplaceProcessedTracks(const bool bGoodResult);

   // Use this to append a NEW output track.
   Track *AddToOutputTracks(const std::shared_ptr<Track> &t);

//
// protected data
//
// The Effect base class will set these variables, some or all of which
// may be needed by any particular subclass of Effect.
//
protected:
   BasicUI::ProgressDialog *mProgress = nullptr; // Temporary pointer, NOT deleted in destructor.
   double         mProjectRate; // Sample rate of the project - NEW tracks should
                               // be created with this rate...
   double         mSampleRate;
   WaveTrackFactory   *mFactory;
   const TrackList *inputTracks() const { return mTracks; }
   const TenacityProject *FindProject() const;
   std::shared_ptr<TrackList> mOutputTracks; // used only if CopyInputTracks() is called.
   double         mT0;
   double         mT1;
#ifdef EXPERIMENTAL_SPECTRAL_EDITING
   double         mF0;
   double         mF1;
#endif
   wxArrayString  mPresetNames;
   wxArrayString  mPresetValues;
   int            mPass;

   // UI
   //! This smart pointer tracks the lifetime of the dialog
   wxWeakRef<wxDialog> mHostUIDialog;
   //! This weak pointer may be the same as the above, or null
   wxWeakRef<wxDialog> mUIDialog;
   wxWindow       *mUIParent;
   unsigned       mUIFlags{ 0 };

   sampleCount    mSampleCnt;

 // Used only by the base Effect class
 //
 private:
   wxString GetSavedStateGroup();
   double GetDefaultDuration();

   void CountWaveTracks();

   // Driver for client effects
   bool ProcessTrack(int count,
                     ChannelNames map,
                     WaveTrack *left,
                     WaveTrack *right,
                     sampleCount start,
                     sampleCount len,
                     FloatBuffers &inBuffer,
                     FloatBuffers &outBuffer,
                     ArrayOf< float * > &inBufPos,
                     ArrayOf< float *> &outBufPos);

 //
 // private data
 //
 // Used only by the base Effect class
 //
private:
   TrackList *mTracks; // the complete list of all tracks

   bool mIsBatch;
   bool mIsLinearEffect;
   bool mPreviewWithNotSelected;
   bool mPreviewFullSelection;

   double mDuration;
   NumericFormatSymbol mDurationFormat;

   bool mIsPreview;

   std::vector<Track*> mIMap;
   std::vector<Track*> mOMap;

   int mNumTracks; //v This is really mNumWaveTracks, per CountWaveTracks() and GetNumWaveTracks().
   int mNumGroups;

   // For client driver
   EffectUIClientInterface *mClient;
   size_t mNumAudioIn;
   size_t mNumAudioOut;

   size_t mBufferSize;
   size_t mBlockSize;
   unsigned mNumChannels;

public:
   const static wxString kUserPresetIdent;
   const static wxString kFactoryPresetIdent;
   const static wxString kCurrentSettingsIdent;
   const static wxString kFactoryDefaultsIdent;
};

// FIXME:
// FIXME:  Remove this once all effects are using the NEW dialog
// FIXME:

#define ID_EFFECT_PREVIEW ePreviewID

// Utility functions

inline float TrapFloat(float x, float min, float max)
{
   if (x <= min)
      return min;

   if (x >= max)
      return max;

   return x;
}

inline double TrapDouble(double x, double min, double max)
{
   if (x <= min)
      return min;

   if (x >= max)
      return max;

   return x;
}

inline long TrapLong(long x, long min, long max)
{
   if (x <= min)
      return min;

   if (x >= max)
      return max;

   return x;
}

// Helper macros for defining, reading and verifying effect parameters

#define Param(name, type, key, def, min, max, scale) \
   static const wxChar * KEY_ ## name = (key); \
   static const type DEF_ ## name = (def); \
   static const type MIN_ ## name = (min); \
   static const type MAX_ ## name = (max); \
   static const type SCL_ ## name = (scale);

#define PBasic(name, type, key, def) \
   static const wxChar * KEY_ ## name = (key); \
   static const type DEF_ ## name = (def);

#define PRange(name, type, key, def, min, max) \
   PBasic(name, type, key, def); \
   static const type MIN_ ## name = (min); \
   static const type MAX_ ## name = (max);

#define PScale(name, type, key, def, min, max, scale) \
   PRange(name, type, key, def, min, max); \
   static const type SCL_ ## name = (scale);

#define ReadParam(type, name) \
   type name = DEF_ ## name; \
   if (!parms.ReadAndVerify(KEY_ ## name, &name, DEF_ ## name, MIN_ ## name, MAX_ ## name)) \
      return false;

#define ReadBasic(type, name) \
   type name; \
   (void)(MIN_ ##name); \
   (void)(MAX_ ##name); \
   (void)(SCL_ ##name); \
   if (!parms.ReadAndVerify(KEY_ ## name, &name, DEF_ ## name)) \
      return false;

#define ReadAndVerifyEnum(name, list, listSize) \
   int name; \
   if (!parms.ReadAndVerify(KEY_ ## name, &name, DEF_ ## name, list, listSize)) \
      return false;

#define ReadAndVerifyEnumWithObsoletes(name, list, listSize, obsoleteList, nObsolete) \
   int name; \
   if (!parms.ReadAndVerify(KEY_ ## name, &name, DEF_ ## name, \
                            list, listSize, obsoleteList, nObsolete)) \
      return false;

#define ReadAndVerifyInt(name) ReadParam(int, name)
#define ReadAndVerifyDouble(name) ReadParam(double, name)
#define ReadAndVerifyFloat(name) ReadParam(float, name)
#define ReadAndVerifyBool(name) ReadBasic(bool, name)
#define ReadAndVerifyString(name) ReadBasic(wxString, name)

#endif
