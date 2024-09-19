/**********************************************************************

  Tenacity

  ExportPlugin.h

  Avery King split from Export.h
  Originally written by Dominic Mazzoni

**********************************************************************/

#pragma once

#include <memory>
#include <vector>

class wxFileName;
class wxString;
class wxWindow;
class TenacityProject;

#include "../Tags.h"
#include "../shuttle/ShuttleGui.h"
#include "../widgets/ProgressDialog.h"

// Tenacity libraries
#include <lib-basic-ui/BasicUI.h>
#include <lib-files/FileNames.h>
#include <lib-sample-track/Mix.h>
#include <lib-strings/Internat.h>

struct TENACITY_DLL_API FormatInfo
{
    FormatInfo() {}
    FormatInfo( const FormatInfo & ) = default;
    FormatInfo &operator = ( const FormatInfo & ) = default;
    ~FormatInfo() {}

    wxString mFormat;
    TranslatableString mDescription;
    // wxString mExtension;
    FileExtensions mExtensions;
    FileNames::FileTypes mMask;
    unsigned mMaxChannels;
    bool mCanMetaData;
};

class TENACITY_DLL_API ExportPlugin /* not final */
{
    public:
        using ProgressResult = BasicUI::ProgressResult;

        ExportPlugin();
        virtual ~ExportPlugin();

        int AddFormat();
        void SetFormat(const wxString & format, int index);
        void SetDescription(const TranslatableString & description, int index);
        void AddExtension(const FileExtension &extension, int index);
        void SetExtensions(FileExtensions extensions, int index);
        void SetMask(FileNames::FileTypes mask, int index);
        void SetMaxChannels(unsigned maxchannels, unsigned index);
        void SetCanMetaData(bool canmetadata, int index);

        virtual int GetFormatCount();
        virtual wxString GetFormat(int index);
        TranslatableString GetDescription(int index);
        /** @brief Return the (first) file name extension for the sub-format.
        * @param index The sub-format for which the extension is wanted */
        virtual FileExtension GetExtension(int index = 0);
        /** @brief Return all the file name extensions used for the sub-format.
        * @param index the sub-format for which the extension is required */
        virtual FileExtensions GetExtensions(int index = 0);
        FileNames::FileTypes GetMask(int index);
        virtual unsigned GetMaxChannels(int index);
        virtual bool GetCanMetaData(int index);

        virtual bool IsExtension(const FileExtension & ext, int index);

        virtual bool DisplayOptions(wxWindow *parent, int format = 0);

        virtual void OptionsCreate(ShuttleGui &S, int format) = 0;

        virtual bool CheckFileName(wxFileName &filename, int format = 0);
        /** @brief Exporter plug-ins may override this to specify the number
        * of channels in exported file. -1 for unspecified */
        virtual int SetNumExportChannels() { return -1; }

        /** \brief called to export audio into a file.
        *
        * @param pDialog To be initialized with pointer to a NEW ProgressDialog if
        * it was null, otherwise gives an existing dialog to be reused
        *  (working around a problem in wxWidgets for Mac; see bug 1600)
        * @param selectedOnly Set to true if all tracks should be mixed, to false
        * if only the selected tracks should be mixed and exported.
        * @param metadata A Tags object that will over-ride the one in *project and
        * be used to tag the file that is exported.
        * @param subformat Control which of the multiple formats this exporter is
        * capable of exporting should be used. Used where a single export plug-in
        * handles a number of related formats, but they have separate
        * entries in the Format drop-down list box. For example, the options to
        * export to "Other PCM", "AIFF 16 Bit" and "WAV 16 Bit" are all the same
        * libsndfile export plug-in, but with subformat set to 0, 1, and 2
        * respectively.
        * @return ProgressResult::Failed or ProgressResult::Cancelled if export
        * fails to complete for any reason, in which case this function is
        * responsible for alerting the user.  Otherwise ProgressResult::Success or
        * ProgressResult::Stopped
        */
        virtual ProgressResult Export(TenacityProject *project,
                                      std::unique_ptr<ProgressDialog> &pDialog,
                                      unsigned channels,
                                      const wxFileNameWrapper &fName,
                                      bool selectedOnly,
                                      double t0,
                                      double t1,
                                      MixerSpec *mixerSpec = NULL,
                                      const Tags *metadata = NULL,
                                      int subformat = 0) = 0;

    protected:
        std::unique_ptr<Mixer> CreateMixer(const TrackList &tracks,
                bool selectionOnly,
                double startTime, double stopTime,
                unsigned numOutChannels, size_t outBufferSize, bool outInterleaved,
                double outRate, sampleFormat outFormat,
                MixerSpec *mixerSpec);

    // Create or recycle a dialog.
    static void InitProgress(std::unique_ptr<ProgressDialog> &pDialog,
            const TranslatableString &title, const TranslatableString &message);
    static void InitProgress(std::unique_ptr<ProgressDialog> &pDialog,
            const wxFileNameWrapper &title, const TranslatableString &message);

    private:
        std::vector<FormatInfo> mFormatInfos;
};

using ExportPluginArray = std::vector< std::unique_ptr< ExportPlugin > >;
