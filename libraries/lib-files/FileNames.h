/**********************************************************************

  Audacity: A Digital Audio Editor

  FileNames.h

  James Crook

**********************************************************************/

#ifndef __AUDACITY_FILE_NAMES__
#define __AUDACITY_FILE_NAMES__

#include <wx/dir.h> // for wxDIR_FILES
#include <wx/string.h> // function return value
#include "Identifier.h"
#include "Prefs.h"

// Please try to support unlimited path length instead of using PLATFORM_MAX_PATH!
// Define one constant for maximum path value, so we don't have to do
// platform-specific conditionals everywhere we want to check it.
#define PLATFORM_MAX_PATH 260 // Play it safe for default, with same value as Windows' MAX_PATH.

#ifdef __WXMAC__
#undef PLATFORM_MAX_PATH
#define PLATFORM_MAX_PATH PATH_MAX
#endif

#ifdef __WXGTK__
// Some systems do not restrict the path length and therefore PATH_MAX is undefined
#ifdef PATH_MAX
#undef PLATFORM_MAX_PATH
#define PLATFORM_MAX_PATH PATH_MAX
#endif
#endif

#ifdef __WXX11__
// wxX11 should also get the platform-specific definition of PLATFORM_MAX_PATH, so do not declare here.
#endif

#ifdef __WXMSW__
#undef PLATFORM_MAX_PATH
#define PLATFORM_MAX_PATH MAX_PATH
#endif

class wxFileName;
class wxFileNameWrapper;

namespace FileNames
{
   // A description of a type of file
   struct FileType {
      FileType() = default;

      FileType( TranslatableString d, FileExtensions e, bool a = false )
         : description{ std::move( d ) }
         , extensions( std::move( e ) )
         , appendExtensions{ a }
      {}

      TranslatableString description;
      FileExtensions extensions;
      // Whether to extend the displayed description with mention of the
      // extensions:
      bool appendExtensions = false;
   };

   // Frequently used types
   extern FILES_API const FileType
        AllFiles // *
      , TenacityProjects // *.aup3
      , DynamicLibraries // depends on the operating system
      , TextFiles // *.txt
      , XMLFiles; // *.xml, *.XML
   
   using FileTypes = std::vector< FileType >;

   // Convert fileTypes into a single string as expected by wxWidgets file
   // selection dialog
   FILES_API wxString FormatWildcard( const FileTypes &fileTypes );

   /** @brief Creates a directory.
    * 
    * If any other directories in the path don't exist, they are also created.
    * Permissions are also set to read and execute for the owner and execute
    * for everything else.
    * 
    * @return Returns the name of the newly created directory or an empty
    * string on failure.
    * 
   */
   FILES_API wxString MkDir(const wxString &Str);

   FILES_API bool IsMidi(const FilePath &fName);

   /** \brief A list of directories that should be searched for Audacity files
    * (plug-ins, help files, etc.).
    *
    * On Unix this will include the directory Audacity was installed into,
    * plus the current user's .audacity-data/Plug-Ins directory.  Additional
    * directories can be specified using the TENACITY_PATH environment
    * variable.  On Windows or Mac OS, this will include the directory
    * which contains the Audacity program. */
   FILES_API const FilePaths &AudacityPathList();
   FILES_API void SetAudacityPathList( FilePaths list );

   // originally an ExportMultipleDialog method. Append suffix if newName appears in otherNames.
   FILES_API void MakeNameUnique(
      FilePaths &otherNames, wxFileName &newName);

   FILES_API wxString LowerCaseAppNameInPath( const wxString & dirIn);
   /** \brief Audacity user data directory
    *
    * Where audacity keeps its settings and other user data squirreled away,
    * by default ~/.audacity-data/ on Unix, Application Data/Audacity on
    * windows system */
   FILES_API FilePath DataDir();
   FILES_API FilePath ResourcesDir();
   FILES_API FilePath HtmlHelpDir();
   FILES_API FilePath HtmlHelpIndexFile(bool quick);
   FILES_API FilePath LegacyChainDir();
   FILES_API FilePath MacroDir();
   FILES_API FilePath NRPDir();
   FILES_API FilePath NRPFile();
   FILES_API FilePath PluginRegistry();
   FILES_API FilePath PluginSettings();

   FILES_API FilePath BaseDir();
   FILES_API FilePath ModulesDir();

   /** \brief The user plug-in directory (not a system one)
    *
    * This returns the string path to where the user may have put plug-ins
    * if they don't have system admin rights. Under default settings, it's
    * <DataDir>/Plug-Ins/ */
   FILES_API FilePath PlugInDir();
   FILES_API FilePath ThemeDir();
   FILES_API FilePath ThemeComponentsDir();
   FILES_API FilePath ThemeCachePng();
   FILES_API FilePath ThemeCacheAsCee();
   FILES_API FilePath ThemeComponent(const wxString &Str);
   FILES_API FilePath ThemeCacheHtm();
   FILES_API FilePath ThemeImageDefsAsCee();

   // Obtain name of loaded module that contains address
   FILES_API FilePath PathFromAddr(void *addr);

   FILES_API bool IsPathAvailable( const FilePath & Path);
   FILES_API wxFileNameWrapper DefaultToDocumentsFolder
      (const wxString &preference);

   // If not None, determines a preference key (for the default path string) to
   // be read and updated
   enum class Operation {
      // _ on None to defeat some macro that is expanding this.
      _None,

      // These do not have a specific pathtype
      Temp,
      Presets,

      // These have default/lastused pathtypes
      Open,
      Save,
      Import,
      Export,
      MacrosOut
   };

   enum class PathType {
      // _ on None to defeat some macro that is expanding this.
      _None,
      User,
      LastUsed
   };

   FILES_API wxString PreferenceKey(FileNames::Operation op, FileNames::PathType type);

   FILES_API FilePath FindDefaultPath(Operation op);
   FILES_API void UpdateDefaultPath(Operation op, const FilePath &path);

   // F is a function taking a wxString, returning wxString
   template<typename F>
   FilePath WithDefaultPath
   (Operation op, const FilePath &defaultPath, F function)
   {
      auto path = gPrefs->Read(PreferenceKey(op, PathType::User), defaultPath);
      if (path.empty())
         path = FileNames::FindDefaultPath(op);
      auto result = function(path);
      FileNames::UpdateDefaultPath(op, ::wxPathOnly(result));
      return result;
   }

   // Useful functions for working with search paths
   FILES_API void AddUniquePathToPathList(const FilePath &path,
                                       FilePaths &pathList);
   FILES_API void AddMultiPathsToPathList(const wxString &multiPathString,
                                       FilePaths &pathList);
   FILES_API void FindFilesInPathList(const wxString & pattern,
                                   const FilePaths & pathList,
                                   FilePaths &results,
                                   int flags = wxDIR_FILES);


   //! Check location on writable access and return true if checked successfully.
   FILES_API bool WritableLocationCheck(const FilePath& path);

   // wxString compare function for sorting case, which is needed to load correctly.
   FILES_API int CompareNoCase(const wxString& first, const wxString& second);

   // Create a unique filename using the passed prefix and suffix
   FILES_API wxString CreateUniqueName(const wxString &prefix,
                             const wxString &suffix = wxEmptyString);

   // File extension used for unsaved/temporary project files
   FILES_API wxString UnsavedProjectExtension();

   FILES_API
   bool IsOnFATFileSystem(const FilePath &path);

   FILES_API
   //! Give enough of the path to identify the device.  (On Windows, drive letter plus ':')
   wxString AbbreviatePath(const wxFileName &fileName);
};

#endif
