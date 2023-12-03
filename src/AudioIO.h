/**********************************************************************

  Audacity: A Digital Audio Editor

  AudioIO.h

  Dominic Mazzoni

  Use the PortAudio library to play and record sound

**********************************************************************/

#ifndef __AUDACITY_AUDIO_IO__
#define __AUDACITY_AUDIO_IO__


#include "AudioIOBase.h" // to inherit
#include "PlaybackSchedule.h" // member variable

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#ifdef EXPERIMENTAL_MIDI_OUT
typedef void PmStream;
typedef int32_t PmTimestamp;

class Alg_seq;
class Alg_event;
class Alg_iterator;

class NoteTrack;
using NoteTrackArray = std::vector < std::shared_ptr< NoteTrack > >;
using NoteTrackConstArray = std::vector < std::shared_ptr< const NoteTrack > >;

#endif // EXPERIMENTAL_MIDI_OUT

#include <wx/event.h> // to declare custom event types

// Tenacity libraries
#include <lib-math/SampleCount.h>
#include <lib-math/SampleFormat.h>
#include <lib-utility/MessageBuffer.h>

class AudioIOBase;
class AudioIO;
class RingBuffer;
class Mixer;
class Resample;
class SelectedRegion;

class TenacityProject;

class WaveTrack;
using WaveTrackArray = std::vector < std::shared_ptr < WaveTrack > >;
using WaveTrackConstArray = std::vector < std::shared_ptr < const WaveTrack > >;

struct PaStreamCallbackTimeInfo;
typedef unsigned long PaStreamCallbackFlags;
typedef int PaError;

bool ValidateDeviceNames();

constexpr int MAX_MIDI_BUFFER_SIZE = 5000;
constexpr int DEFAULT_SYNTH_LATENCY = 5;

wxDECLARE_EXPORTED_EVENT(TENACITY_DLL_API,
                         EVT_AUDIOIO_PLAYBACK, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(TENACITY_DLL_API,
                         EVT_AUDIOIO_CAPTURE, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(TENACITY_DLL_API,
                         EVT_AUDIOIO_MONITOR, wxCommandEvent);

// PRL:
// If we always run a portaudio output stream (even just to produce silence)
// whenever we play Midi, then we might use just one thread for both.
// I thought this would improve MIDI synch problems on Linux/ALSA, but RBD
// convinced me it was neither a necessary nor sufficient fix.  Perhaps too the
// MIDI thread might block in some error situations but we should then not
// also block the audio thread.
// So leave the separate thread ENABLED.
#define USE_MIDI_THREAD

struct TransportTracks {
   WaveTrackArray playbackTracks;
   WaveTrackArray captureTracks;
#ifdef EXPERIMENTAL_MIDI_OUT
   NoteTrackConstArray midiTracks;
#endif

   // This is a subset of playbackTracks
   WaveTrackConstArray prerollTracks;
};

// This workaround makes pause and stop work when output is to GarageBand,
// which seems not to implement the notes-off message correctly.
#define AUDIO_IO_GB_MIDI_WORKAROUND

/** brief The function which is called from PortAudio's callback thread
 * context to collect and deliver audio for / from the sound device.
 *
 * This covers recording, playback, and doing both simultaneously. It is
 * also invoked to do monitoring and software playthrough. Note that dealing
 * with the two buffers needs some care to ensure that the right things
 * happen for all possible cases.
 * @param inputBuffer Buffer of length framesPerBuffer containing samples
 * from the sound card, or null if not capturing audio. Note that the data
 * type will depend on the format of audio data that was chosen when the
 * stream was created (so could be floats or various integers)
 * @param outputBuffer Uninitialised buffer of length framesPerBuffer which
 * will be sent to the sound card after the callback, or null if not playing
 * audio back.
 * @param framesPerBuffer The length of the playback and recording buffers
 * @param PaStreamCallbackTimeInfo Pointer to PortAudio time information
 * structure, which tells us how long we have been playing / recording
 * @param statusFlags PortAudio stream status flags
 * @param userData pointer to user-defined data structure. Provided for
 * flexibility by PortAudio, but not used by Audacity - the data is stored in
 * the AudioIO class instead.
 */
int audacityAudioCallback(
   const void *inputBuffer, void *outputBuffer,
   unsigned long framesPerBuffer,
   const PaStreamCallbackTimeInfo *timeInfo,
   PaStreamCallbackFlags statusFlags, void *userData );

class TENACITY_DLL_API AudioIoCallback /* not final */
   : public AudioIOBase
{
public:
   AudioIoCallback();
   ~AudioIoCallback();

public:
   // This function executes in a thread spawned by the PortAudio library
   int AudioCallback(
      constSamplePtr inputBuffer, float *outputBuffer,
      unsigned long framesPerBuffer,
      const PaStreamCallbackTimeInfo *timeInfo,
      const PaStreamCallbackFlags statusFlags, void *userData);

#ifdef EXPERIMENTAL_MIDI_OUT
   void PrepareMidiIterator(bool send = true, double offset = 0);
   bool StartPortMidiStream();

   // Compute nondecreasing real time stamps, accounting for pauses, but not the
   // synth latency.
   double UncorrectedMidiEventTime();

   void OutputEvent();
   void FillMidiBuffers();
   void GetNextEvent();
   double PauseTime();
   void AllNotesOff(bool looping = false);

   /** \brief Compute the current PortMidi timestamp time.
    *
    * This is used by PortMidi to synchronize midi time to audio samples
    */
   PmTimestamp MidiTime();

   // Note: audio code solves the problem of soloing/muting tracks by scanning
   // all playback tracks on every call to the audio buffer fill routine.
   // We do the same for Midi, but it seems wasteful for at least two
   // threads to be frequently polling to update status. This could be
   // eliminated (also with a reduction in code I think) by updating mHasSolo
   // each time a solo button is activated or deactivated. For now, I'm
   // going to do this polling in the FillMidiBuffer routine to localize
   // changes for midi to the midi code, but I'm declaring the variable
   // here so possibly in the future, Audio code can use it too. -RBD
 private:
   bool  mHasSolo; // is any playback solo button pressed?
 public:
   bool SetHasSolo(bool hasSolo);
   bool GetHasSolo() { return mHasSolo; }
#endif

   std::shared_ptr< AudioIOListener > GetListener() const
      { return mListener.lock(); }
   void SetListener( const std::shared_ptr< AudioIOListener > &listener);
   
   // Part of the callback
   int CallbackDoSeek();

   // Part of the callback
   void CallbackCheckCompletion(
      int &callbackReturn, unsigned long len);

   int mbHasSoloTracks;
   int mCallbackReturn;
   // Helpers to determine if tracks have already been faded out.
   unsigned  CountSoloingTracks();
   bool TrackShouldBeSilent( const WaveTrack &wt );
   bool TrackHasBeenFadedOut( const WaveTrack &wt );
   bool AllTracksAlreadySilent();

   // These eight functions do different parts of AudioCallback().
   void ComputeMidiTimings(
      const PaStreamCallbackTimeInfo *timeInfo,
      unsigned long framesPerBuffer);
   void CheckSoundActivatedRecordingLevel(
      float *inputSamples,
      unsigned long framesPerBuffer
   );
   void AddToOutputChannel( unsigned int chan,
      float * outputMeterFloats,
      float * outputFloats,
      const float * tempBuf,
      bool drop,
      unsigned long len,
      WaveTrack& vt
      );
   bool FillOutputBuffers(
      float *outputBuffer,
      unsigned long framesPerBuffer,
      float *outputMeterFloats
   );
   void FillInputBuffers(
      constSamplePtr inputBuffer, 
      unsigned long framesPerBuffer,
      const PaStreamCallbackFlags statusFlags,
      float * tempFloats
   );
   void UpdateTimePosition(
      unsigned long framesPerBuffer
   );
   void DoPlaythrough(
      constSamplePtr inputBuffer, 
      float *outputBuffer,
      unsigned long framesPerBuffer,
      float *outputMeterFloats
   );
   void SendVuInputMeterData(
      const float *inputSamples,
      unsigned long framesPerBuffer
   );
   void SendVuOutputMeterData(
      const float *outputMeterFloats,
      unsigned long framesPerBuffer
   );

protected:

   // Buffers
   std::vector<WaveTrack*> mTrackChannelsBuffer;
   std::vector<float*>     mScratchBuffers;
   AutoAllocator<float>    mScratchBufferAllocator;
   std::shared_ptr<float>  mTemporaryBuffer;

   // Bufer preparation status
   bool mBuffersPrepared;

public:

   /// @brief Reallocate all buffers to their new sizes.
   void UpdateBuffers();

// Required by these functions...
#ifdef EXPERIMENTAL_MIDI_OUT
   double AudioTime() { return mPlaybackSchedule.mT0 + mNumFrames / mRate; }
#endif


   /** \brief Get the number of audio samples ready in all of the playback
   * buffers.
   *
   * Returns the smallest of the buffer ready space values in the event that
   * they are different. */
   size_t GetCommonlyReadyPlayback();


#ifdef EXPERIMENTAL_MIDI_OUT
   //   MIDI_PLAYBACK:
   PmStream        *mMidiStream;
   int              mLastPmError;

   /// Latency of MIDI synthesizer
   long             mSynthLatency; // ms

   // These fields are used to synchronize MIDI with audio:

   /// Number of frames output, including pauses
   volatile long    mNumFrames;
   /// How many frames of zeros were output due to pauses?
   volatile long    mNumPauseFrames;
   /// total of backward jumps
   volatile int     mMidiLoopPasses;
   inline double MidiLoopOffset() {
      return mMidiLoopPasses * (mPlaybackSchedule.mT1 - mPlaybackSchedule.mT0);
   }

   volatile long    mAudioFramesPerBuffer;
   /// Used by Midi process to record that pause has begun,
   /// so that AllNotesOff() is only delivered once
   volatile bool    mMidiPaused;
   /// The largest timestamp written so far, used to delay
   /// stream closing until last message has been delivered
   PmTimestamp mMaxMidiTimestamp;

   /// Offset from ideal sample computation time to system time,
   /// where "ideal" means when we would get the callback if there
   /// were no scheduling delays or computation time
   double mSystemMinusAudioTime;
   /// audio output latency reported by PortAudio
   /// (initially; for Alsa, we adjust it to the largest "observed" value)
   double mAudioOutLatency;

   // Next two are used to adjust the previous two, if
   // PortAudio does not provide the info (using ALSA):

   /// time of first callback
   /// used to find "observed" latency
   double mStartTime;
   /// number of callbacks since stream start
   long mCallbackCount;

   /// Make just one variable to communicate from audio to MIDI thread,
   /// to avoid problems of atomicity of updates
   volatile double mSystemMinusAudioTimePlusLatency;

   Alg_seq      *mSeq;
   std::unique_ptr<Alg_iterator> mIterator;
   /// The next event to play (or null)
   Alg_event    *mNextEvent;

#ifdef AUDIO_IO_GB_MIDI_WORKAROUND
   std::vector< std::pair< int, int > > mPendingNotesOff;
#endif

   /// Real time at which the next event should be output, measured in seconds.
   /// Note that this could be a note's time+duration for note offs.
   double           mNextEventTime;
   /// Track of next event
   NoteTrack        *mNextEventTrack;
   /// Is the next event a note-on?
   bool             mNextIsNoteOn;
   /// when true, mSendMidiState means send only updates, not note-on's,
   /// used to send state changes that precede the selected notes
   bool             mSendMidiState;
   NoteTrackConstArray mMidiPlaybackTracks;
#endif

   ArrayOf<std::unique_ptr<Resample>> mResample;
   ArrayOf<std::unique_ptr<RingBuffer>> mCaptureBuffers;
   WaveTrackArray      mCaptureTracks;
   ArrayOf<std::unique_ptr<RingBuffer>> mPlaybackBuffers;
   WaveTrackArray      mPlaybackTracks;

   ArrayOf<std::unique_ptr<Mixer>> mPlaybackMixers;
   static int          mNextStreamToken;
   double              mFactor;
   unsigned long       mMaxFramesOutput; // The actual number of frames output.
   bool                mbMicroFades; 

   double              mSeek;
   double              mPlaybackRingBufferSecs;
   double              mCaptureRingBufferSecs;

   /// Preferred batch size for replenishing the playback RingBuffer
   size_t              mPlaybackSamplesToCopy;
   /// Occupancy of the queue we try to maintain, with bigger batches if needed
   size_t              mPlaybackQueueMinimum;

   double              mMinCaptureSecsToCopy;
   bool                mSoftwarePlaythrough;
   /// True if Sound Activated Recording is enabled
   bool                mPauseRec;
   float               mSilenceLevel;
   unsigned int        mNumCaptureChannels{ 0 };
   unsigned int        mNumPlaybackChannels{ 0 };
   sampleFormat        mCaptureFormat;
   unsigned long long  mLostSamples{ 0 };
   volatile bool       mAudioThreadShouldCallFillBuffersOnce;
   volatile bool       mAudioThreadFillBuffersLoopRunning;
   volatile bool       mAudioThreadFillBuffersLoopActive;

   wxLongLong          mLastPlaybackTimeMillis;

#ifdef EXPERIMENTAL_MIDI_OUT
   volatile bool       mMidiThreadFillBuffersLoopRunning;
   volatile bool       mMidiThreadFillBuffersLoopActive;
#endif

   volatile double     mLastRecordingOffset;
   PaError             mLastPaError;

protected:

   /** @brief Convert's the user's latency preference to samples.
    * 
    * Internally, we use number of samples instead of milliseconds as that's
    * just a preference based on what unit the user wants shown.
    * 
    * @return Returns the converted latency preference.
    * 
   */
   long GetConvertedLatencyPreference();

   bool                mUpdateMeters;
   volatile bool       mUpdatingMeters;

   std::weak_ptr< AudioIOListener > mListener;

   bool mUsingAlsa { false };

   // For cacheing supported sample rates
   static double mCachedBestRateOut;
   static bool mCachedBestRatePlaying;
   static bool mCachedBestRateCapturing;

   // Serialize main thread and PortAudio thread's attempts to pause and change
   // the state used by the third, Audio thread.
   std::mutex mSuspendAudioThread;

#ifdef EXPERIMENTAL_SCRUBBING_SUPPORT
public:
   struct ScrubState;
   std::unique_ptr<ScrubState> mScrubState;

   bool mSilentScrub;
   double mScrubSpeed;
   sampleCount mScrubDuration;
#endif

protected:
   // A flag tested and set in one thread, cleared in another.  Perhaps
   // this guarantee of atomicity is more cautious than necessary.
   std::atomic<int> mRecordingException {};
   void SetRecordingException()
   {
      mRecordingException++;
   }

   void ClearRecordingException()
   {
      if (mRecordingException)
      {
         mRecordingException--;
      }
   }

   std::vector< std::pair<double, double> > mLostCaptureIntervals;
   bool mDetectDropouts{ true };

public:
   // Pairs of starting time and duration
   const std::vector< std::pair<double, double> > &LostCaptureIntervals()
   { return mLostCaptureIntervals; }

   // Used only for testing purposes in alpha builds
   bool mSimulateRecordingErrors{ false };

   // Whether to check the error code passed to audacityAudioCallback to
   // detect more dropouts
   bool mDetectUpstreamDropouts{ true };

protected:
   RecordingSchedule mRecordingSchedule{};

   // Another circular buffer
   // Holds track time values corresponding to every nth sample in the playback
   // buffers, for some large n
   struct TimeQueue {
      Doubles mData;
      size_t mSize{ 0 };
      double mLastTime {};
      // These need not be updated atomically, because we rely on the atomics
      // in the playback ring buffers to supply the synchronization.  Still,
      // align them to avoid false sharing.
      struct Cursor {
         size_t mIndex {};
         size_t mRemainder {};
      };
      NonInterfering<Cursor> mHead, mTail;

      void Producer(
         const PlaybackSchedule &schedule, double rate, double scrubSpeed,
         size_t nSamples );
      double Consumer( size_t nSamples, double rate );
   } mTimeQueue;

   PlaybackSchedule mPlaybackSchedule;
};

class TENACITY_DLL_API AudioIO final
   : public AudioIoCallback
{

   AudioIO();
   ~AudioIO();

public:
   // This might return null during application startup or shutdown
   static AudioIO *Get();

   /** \brief Start up Portaudio for capture and recording as needed for
    * input monitoring and software playthrough only
    *
    * This uses the Default project sample format, current sample rate, and
    * selected number of input channels to open the recording device and start
    * reading input data. If software playthrough is enabled, it also opens
    * the output device in stereo to play the data through */
   void StartMonitoring( const AudioIOStartStreamOptions &options );

   /** \brief Start recording or playing back audio
    *
    * Allocates buffers for recording and playback, gets the Audio thread to
    * fill them, and sets the stream rolling.
    * If successful, returns a token identifying this particular stream
    * instance.  For use with IsStreamActive() */

   int StartStream(const TransportTracks &tracks,
                   double t0, double t1,
                   const AudioIOStartStreamOptions &options);

   /** \brief Stop recording, playback or input monitoring.
    *
    * Does quite a bit of housekeeping, including switching off monitoring,
    * flushing recording buffers out to wave tracks, and applies latency
    * correction to recorded tracks if necessary */
   void StopStream() override;
   /** \brief Move the playback / recording position of the current stream
    * by the specified amount from where it is now */
   void SeekStream(double seconds) { mSeek = seconds; }

   using PostRecordingAction = std::function<void()>;
   
   //! Enqueue action for main thread idle time, not before the end of any recording in progress
   /*! This may be called from non-main threads */
   void CallAfterRecording(PostRecordingAction action);

#ifdef EXPERIMENTAL_SCRUBBING_SUPPORT
   bool IsScrubbing() const { return IsBusy() && mScrubState != 0; }

   /** \brief Notify scrubbing engine of desired position or speed.
   * If options.adjustStart is true, then when mouse movement exceeds maximum
   * scrub speed, adjust the beginning of the scrub interval rather than the
   * end, so that the scrub skips or "stutters" to stay near the cursor.
   */
   void UpdateScrub(double endTimeOrSpeed, const ScrubbingOptions &options);

   void StopScrub();

   /** \brief return the ending time of the last scrub interval.
   */
   double GetLastScrubTime() const;
#endif

public:
   std::string LastPaErrorString();

   wxLongLong GetLastPlaybackTime() const { return mLastPlaybackTimeMillis; }
   std::shared_ptr<TenacityProject> GetOwningProject() const
   { return mOwningProject.lock(); }

   /** \brief Pause and un-pause playback and recording */
   void SetPaused(bool state);

   sampleFormat GetCaptureFormat() { return mCaptureFormat; }
   unsigned GetNumPlaybackChannels() const { return mNumPlaybackChannels; }
   unsigned GetNumCaptureChannels() const { return mNumCaptureChannels; }

   // Meaning really capturing, not just pre-rolling
   bool IsCapturing() const;

   /** \brief Ensure selected device names are valid
    *
    */
   static bool ValidateDeviceNames(const std::string &play, const std::string &rec);

   /** \brief Function to automatically set an acceptable volume
    *
    */

   bool IsAvailable(TenacityProject &project) const;

   /** \brief Return a valid sample rate that is supported by the current I/O
   * device(s).
   *
   * The return from this function is used to determine the sample rate that
   * audacity actually runs the audio I/O stream at. if there is no suitable
   * rate available from the hardware, it returns 0.
   * The sampleRate argument gives the desired sample rate (the rate of the
   * audio to be handled, i.e. the currently Project Rate).
   * capturing is true if the stream is capturing one or more audio channels,
   * and playing is true if one or more channels are being played. */
   double GetBestRate(bool capturing, bool playing, double sampleRate);

   /** \brief During playback, the track time most recently played
    *
    * When playing looped, this will start from t0 again,
    * too. So the returned time should be always between
    * t0 and t1
    */
   double GetStreamTime();

   friend void StartAudioIOThread();

#ifdef EXPERIMENTAL_MIDI_OUT
   friend void StartMidiIOThread();
#endif

   static void Init();
   static void Deinit();

   /*! For purposes of CallAfterRecording, treat time from now as if
    recording (when argument is true) or not necessarily so (false) */
   void DelayActions(bool recording);

private:

   bool DelayingActions() const;

   /** \brief Set the current VU meters - this should be done once after
    * each call to StartStream currently */
   void SetMeters();

   /** \brief Opens the portaudio stream(s) used to do playback or recording
    * (or both) through.
    *
    * The sampleRate passed is the Project Rate of the active project. It may
    * or may not be actually supported by playback or recording hardware
    * currently in use (for many reasons). The number of Capture and Playback
    * channels requested includes an allocation for doing software playthrough
    * if necessary. The captureFormat is used for recording only, the playback
    * being floating point always. Returns true if the stream opened successfully
    * and false if it did not. */
   bool StartPortAudioStream(const AudioIOStartStreamOptions &options,
                             unsigned int numPlaybackChannels,
                             unsigned int numCaptureChannels,
                             sampleFormat captureFormat);
   void FillBuffers();

   /** \brief Get the number of audio samples free in all of the playback
   * buffers.
   *
   * Returns the smallest of the buffer free space values in the event that
   * they are different. */
   size_t GetCommonlyFreePlayback();

   /** \brief Get the number of audio samples ready in all of the recording
    * buffers.
    *
    * Returns the smallest of the number of samples available for storage in
    * the recording buffers (i.e. the number of samples that can be read from
    * all record buffers without underflow). */
   size_t GetCommonlyAvailCapture();

   /** \brief Allocate RingBuffer structures, and others, needed for playback
     * and recording.
     *
     * Returns true iff successful.
     */
   bool AllocateBuffers(
      const AudioIOStartStreamOptions &options,
      const TransportTracks &tracks, double t0, double t1, double sampleRate,
      bool scrubbing );

   /** \brief Clean up after StartStream if it fails.
     *
     * If bOnlyBuffers is specified, it only cleans up the buffers. */
   void StartStreamCleanup(bool bOnlyBuffers = false);

   std::mutex mPostRecordingActionMutex;
   PostRecordingAction mPostRecordingAction;
   bool mDelayingActions{ false };
};

static constexpr unsigned ScrubPollInterval_ms = 50;

#endif
