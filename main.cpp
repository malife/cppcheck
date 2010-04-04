
/**
 * C/C++ Checking
 *
 * This program will check either the file(s) specified or all source files in
 * all sub-directories
 *
 * Some checks are commented out below because they generate too many false
 * positives. There are cases when the reported message is true, but it is
 * what the developer wanted to do (missing break after case).
 * It is safe to uncomment any of the checks.
 *
 * Design
 *   The token list is a stringlist with the same contents and structure
 *   as the file.
 *   All checks will check for errors in the token list.
 *
 **/


#include "tokenize.h"   // <- Tokenizer
#include "CommonCheck.h"

#include "CheckMemoryLeak.h"
#include "CheckBufferOverrun.h"
#include "CheckClass.h"
#include "CheckHeaders.h"
#include "CheckOther.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstring>


#ifdef __GNUC__
#include <glob.h>
#include <unistd.h>
#endif
#ifdef __BORLANDC__
#include <dir.h>
#endif
#ifdef _MSC_VER
#include <windows.h>
#endif

//---------------------------------------------------------------------------
bool Debug = false;
bool ShowAll = false;
bool CheckCodingStyle = false;
//---------------------------------------------------------------------------

static void CppCheck(const char FileName[], unsigned int FileId);


static void AddFiles( std::vector<std::string> &filenames, const char path[], const char pattern[] )
{
    #ifdef __GNUC__
    glob_t glob_results;
    glob(pattern, 0, 0, &glob_results);
    for ( unsigned int i = 0; i < glob_results.gl_pathc; i++ )
    {
        std::ostringstream fname;
        fname << path << glob_results.gl_pathv[i];
        filenames.push_back( fname.str() );
    }
    globfree(&glob_results);
    #endif
    #ifdef __BORLANDC__
    struct ffblk f;
    for ( int done = findfirst(pattern, &f, 0); ! done; done = findnext(&f) )
    {
        std::ostringstream fname;
        fname << path << f.ff_name;
        filenames.push_back( fname.str() );
    }
    findclose(&f);
    #endif
    #ifdef _MSC_VER
    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(pattern, &ffd);
	if (INVALID_HANDLE_VALUE != hFind) 
	{
		do
		{
	        std::ostringstream fname;
			fname << path << ffd.cFileName;
			filenames.push_back( fname.str() );
		}
		while (FindNextFile(hFind, &ffd) != 0);
	}
	#endif
}

static void RecursiveAddFiles( std::vector<std::string> &filenames, const char path[] )
{
    AddFiles( filenames, path, "*.cpp" );
    AddFiles( filenames, path, "*.cc" );
    AddFiles( filenames, path, "*.c" );

    #ifdef __GNUC__
    // gcc / cygwin..
    glob_t glob_results;
    glob("*", GLOB_MARK, 0, &glob_results);
    for ( unsigned int i = 0; i < glob_results.gl_pathc; i++ )
    {
        const char *dirname = glob_results.gl_pathv[i];
        if ( dirname[0] == '.' )
            continue;

        if ( strchr(dirname, '/') == 0 )
            continue;

        chdir( dirname );
        std::ostringstream curdir;
        curdir << path << dirname;
        RecursiveAddFiles( filenames, curdir.str().c_str() );
        chdir( ".." );
    }
    globfree(&glob_results);
    #endif
    #ifdef __BORLANDC__
    struct ffblk f ;
    for ( int done = findfirst("*", &f, FA_DIREC); ! done; done = findnext(&f) )
    {
        if ( f.ff_attrib != FA_DIREC || f.ff_name[0] == '.' )
            continue;
        chdir( f.ff_name );
        std::ostringstream curdir;
        curdir << path << f.ff_name << "/";
        RecursiveAddFiles( filenames, curdir.str().c_str() );
        chdir( ".." );
    }
    findclose(&f);
    #endif
    #ifdef _MSC_VER
    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile("*", &ffd);
	if (INVALID_HANDLE_VALUE != hFind) 
	{
		do
		{
			if ( (ffd.cFileName[0]!='.') &&
				 (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
			{
				SetCurrentDirectory( ffd.cFileName );
				std::ostringstream curdir;
				curdir << path << ffd.cFileName << "/";
				RecursiveAddFiles( filenames, curdir.str().c_str() );
				SetCurrentDirectory( ".." );
			}
		}
		while (FindNextFile(hFind, &ffd) != 0);
	}
	#endif
}

//---------------------------------------------------------------------------
// Main function of cppcheck
//---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    std::vector<std::string> filenames;

    bool Recursive = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i],"--debug") == 0)
            Debug = true;

        // Show all messages
        else if (strcmp(argv[i],"--all") == 0)
            ShowAll = true;

        // Checking coding style.
        else if (strcmp(argv[i],"--style")==0)
            CheckCodingStyle = true;

        else if (strcmp(argv[i],"--recursive")==0)
            Recursive = true;

        else if (strchr(argv[i],'*'))
        {
            AddFiles( filenames, "", argv[i] );
        }

        else
        {
            filenames.push_back( argv[i] );
        }
    }

    // No filename given.. automaticly search for available files.
    if ( Recursive )
        RecursiveAddFiles( filenames, "" );

    if (filenames.empty())
    {
        std::cout << "C/C++ code checking.\n"
                     "\n"
                     "Syntax:\n"
                     "    cppcheck [--all] [--style] [--recursive] [filename1] [filename2]\n"
                     "\n"
                     "Options:\n"
                     "    --all    Normally a message is only shown if cppcheck is sure\n"
                     "             it has found a bug.\n"
                     "             When this option is given, all messages are shown.\n"
                     "\n"
                     "    --style  Check coding style.\n"
                     "    --recursive  Recursively check all *.cpp, *.cc and *.c files\n";
        return 0;
    }

    std::sort( filenames.begin(), filenames.end() );

    for (unsigned int c = 0; c < filenames.size(); c++)
    {
        errout.str("");
        CppCheck(filenames[c].c_str(), c);
        std::cerr << errout.str();
    }

    if ( CheckCodingStyle && filenames.size() > 1 )
    {
        errout.str("");
        std::cout << "Checking usage of global functions..\n";
        CheckGlobalFunctionUsage(filenames);
        if ( ! errout.str().empty() )
        {
            std::cerr << "\n";
            std::cerr << errout.str();
        }
    }

    return 0;
}

//---------------------------------------------------------------------------
// CppCheck - A function that checks a specified file
//---------------------------------------------------------------------------

static void CppCheck(const char FileName[], unsigned int FileId)
{
    OnlyReportUniqueErrors = true;

    std::cout << "Checking " << FileName << "...\n";

    // Tokenize the file
    tokens = tokens_back = NULL;
    Files.clear();
    Tokenize(FileName);

    FillFunctionList(FileId);

    // Including header which is not needed (too many false positives)
    WarningIncludeHeader();

    // Clean up tokens..
    DeallocateTokens();

    if ( errout.str().empty() )
        std::cout << "No errors found\n";
}
//---------------------------------------------------------------------------




