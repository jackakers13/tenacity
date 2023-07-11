/**********************************************************************

  Audacity: A Digital Audio Editor

  AboutDialog.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_ABOUT_DLG__
#define __AUDACITY_ABOUT_DLG__

#include <vector>
#include "widgets/wxPanelWrapper.h" // to inherit

class wxStaticBitmap;
class wxTextOutputStream;

class ShuttleGui;

struct AboutDialogCreditItem {
   AboutDialogCreditItem( TranslatableString str, int r )
      : description{ std::move( str ) }, role{ r }
   {}
   TranslatableString description;
   int role;
};

using AboutDialogCreditItemsList = std::vector<AboutDialogCreditItem>;

class TENACITY_DLL_API AboutDialog final : public wxDialogWrapper {

 public:
   AboutDialog(wxWindow * parent);
   virtual ~ AboutDialog();

   static AboutDialog *ActiveInstance();

   wxStaticBitmap *icon;

 private:
   enum Role {
      // Tenacity Roles
      roleTeamMember,
      roleContributor,
      roleEmeritus,
      roleGraphics,
      roleLibrary,
      roleThanks,

      // Regular Audacity team member roles
      roleAudacityTeamMember,
      roleAudacityEmeritusTeam,
      roleAudacityDeceased,
      roleAudacityContributor,
      roleAudacityGraphics,
      roleAudacityThanks
   };

   AboutDialogCreditItemsList creditItems;
   void PopulateAudacityPage( ShuttleGui & S );
   void PopulateLicensePage( ShuttleGui & S );
   void PopulateInformationPage (ShuttleGui & S );

   void CreateCreditsList();
   void AddCredit( const wxString &name, Role role );
   void AddCredit( const wxString &name, TranslatableString format, Role role );
   wxString GetCreditsByRole(AboutDialog::Role role);

   void AddBuildinfoRow( wxTextOutputStream *str, const wxChar * libname,
      const TranslatableString &libdesc, const TranslatableString &status);
   void AddBuildinfoRow( wxTextOutputStream *str,
      const TranslatableString &description, const wxChar *spec);
};
#endif
