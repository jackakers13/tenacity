/*!********************************************************************
*
 Audacity: A Digital Audio Editor

 WaveTrackAffordanceControls.h

 Vitaly Sverchinsky

 **********************************************************************/

#pragma once

#include <wx/font.h>
#include <wx/event.h>

#include "../../../ui/CommonTrackPanelCell.h"
#include "../../../ui/TextEditHelper.h"
#include "../../../ui/SelectHandle.h"

struct TrackListEvent;

class AffordanceHandle;
class SelectHandle;
class WaveClip;
class TrackPanelResizeHandle;
class WaveClipTitleEditHandle;
class WaveTrackAffordanceHandle;
class WaveClipTrimHandle;
class TrackList;

//Handles clip movement, selection, navigation and
//allow name change
class TENACITY_DLL_API WaveTrackAffordanceControls : 
    public CommonTrackCell,
    public TextEditDelegate,
    public wxEvtHandler,
    public std::enable_shared_from_this<WaveTrackAffordanceControls>
{
    std::weak_ptr<WaveClip> mFocusClip;
    std::weak_ptr<WaveTrackAffordanceHandle> mAffordanceHandle;
    std::weak_ptr<TrackPanelResizeHandle> mResizeHandle;
    std::weak_ptr<WaveClipTitleEditHandle> mTitleEditHandle;
    std::weak_ptr<SelectHandle> mSelectHandle;
    std::weak_ptr<WaveClipTrimHandle> mClipTrimHandle;

    std::weak_ptr<WaveClip> mEditedClip;
    std::shared_ptr<TextEditHelper> mTextEditHelper;

    wxFont mClipNameFont;

    //Helper flag, checked when text editing is triggered (and dialog-edit option is disabled)
    bool mClipNameVisible { false };

public:
    WaveTrackAffordanceControls(const std::shared_ptr<Track>& pTrack);

    std::vector<UIHandlePtr> HitTest(const TrackPanelMouseState& state, const TenacityProject* pProject) override;

    void Draw(TrackPanelDrawingContext& context, const wxRect& rect, unsigned iPass) override;

    //Invokes name editing for a clip that currently is
    //in focus(as a result of hit testing), returns true on success
    //false if there is no focus
    bool StartEditClipName(TenacityProject* project);

    std::weak_ptr<WaveClip> GetSelectedClip() const;

    unsigned CaptureKey
    (wxKeyEvent& event, ViewInfo& viewInfo, wxWindow* pParent,
        TenacityProject* project) override;
    
    unsigned KeyDown (wxKeyEvent& event, ViewInfo& viewInfo, wxWindow* pParent,
        TenacityProject* project) override;

    unsigned Char
    (wxKeyEvent& event, ViewInfo& viewInfo, wxWindow* pParent,
        TenacityProject* project) override;

    unsigned LoseFocus(TenacityProject *project) override;

    void OnTextEditFinished(TenacityProject* project, const wxString& text) override;
    void OnTextEditCancelled(TenacityProject* project) override;
    void OnTextModified(TenacityProject* project, const wxString& text) override;
    void OnTextContextMenu(TenacityProject* project, const wxPoint& position) override;

    bool StartEditNameOfMatchingClip( TenacityProject &project,
        std::function<bool(WaveClip&)> test /*!<
            Edit the first clip in the track's list satisfying the test */
    );

    unsigned OnAffordanceClick(const TrackPanelMouseEvent& event, TenacityProject* project);

    bool OnTextCopy(TenacityProject& project);
    bool OnTextCut(TenacityProject& project);
    bool OnTextPaste(TenacityProject& project);
    bool OnTextSelect(TenacityProject& project);

private:
    void ResetClipNameEdit();

    void OnTrackChanged(TrackListEvent& evt);

    unsigned ExitTextEditing();

    std::shared_ptr<TextEditHelper> MakeTextEditHelper(const wxString& text);
};
