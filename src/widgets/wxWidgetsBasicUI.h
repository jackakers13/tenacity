/*!********************************************************************

Audacity: A Digital Audio Editor

@file wxWidgetsBasicUI.h
@brief Implementation of BasicUI using wxWidgets

Paul Licameli

**********************************************************************/
#ifndef __WXWIDGETS_BASIC_UI__
#define __WXWIDGETS_BASIC_UI__

// Tenacity libraries
#include <lib-basic-ui/BasicUI.h>

class wxWindow;

//! Window placement information for wxWidgetsBasicUI can be constructed from a wxWindow pointer
struct TENACITY_DLL_API wxWidgetsWindowPlacement final
: BasicUI::WindowPlacement {
   wxWidgetsWindowPlacement() = default;
   explicit wxWidgetsWindowPlacement( wxWindow *pWindow )
      : pWindow{ pWindow }
   {}

   ~wxWidgetsWindowPlacement() override;
   wxWindow *pWindow{};
};

//! An implementation of BasicUI::Services in terms of the wxWidgets toolkit
/*! This is a singleton that doesn't need TENACITY_DLL_API visibility */
class wxWidgetsBasicUI final : public BasicUI::Services {
public:
   ~wxWidgetsBasicUI() override;

protected:
   void DoCallAfter(const BasicUI::Action &action) override;
   void DoYield() override;
   void DoShowErrorDialog(const BasicUI::WindowPlacement &placement,
      const TranslatableString &dlogTitle,
      const TranslatableString &message,
      const ManualPageID &helpPage,
      const BasicUI::ErrorDialogOptions &options) override;
   BasicUI::MessageBoxResult DoMessageBox(
      const TranslatableString &message,
      BasicUI::MessageBoxOptions options) override;
   std::unique_ptr<BasicUI::ProgressDialog>
   DoMakeProgress(const TranslatableString & title,
      const TranslatableString &message,
      unsigned flags,
      const TranslatableString &remainingLabelText) override;
   std::unique_ptr<BasicUI::GenericProgressDialog>
   DoMakeGenericProgress(const BasicUI::WindowPlacement &placement,
      const TranslatableString &title,
      const TranslatableString &message) override;
   int DoMultiDialog(const TranslatableString &message,
      const TranslatableString &title,
      const TranslatableStrings &buttons,
      const ManualPageID &helpPage,
      const TranslatableString &boxMsg, bool log) override;
};

#endif
