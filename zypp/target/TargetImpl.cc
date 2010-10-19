/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/target/TargetImpl.cc
 *
*/
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <set>

#include <sys/types.h>
#include <dirent.h>

#include "zypp/base/LogTools.h"
#include "zypp/base/Exception.h"
#include "zypp/base/Iterator.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/IOStream.h"
#include "zypp/base/Functional.h"
#include "zypp/base/UserRequestException.h"

#include "zypp/ZConfig.h"
#include "zypp/ZYppFactory.h"

#include "zypp/PoolItem.h"
#include "zypp/ResObjects.h"
#include "zypp/Url.h"
#include "zypp/TmpPath.h"
#include "zypp/RepoStatus.h"
#include "zypp/ExternalProgram.h"
#include "zypp/Repository.h"

#include "zypp/ResFilters.h"
#include "zypp/HistoryLog.h"
#include "zypp/target/TargetImpl.h"
#include "zypp/target/TargetCallbackReceiver.h"
#include "zypp/target/rpm/librpmDb.h"
#include "zypp/target/CommitPackageCache.h"

#include "zypp/parser/ProductFileReader.h"

#include "zypp/pool/GetResolvablesToInsDel.h"
#include "zypp/solver/detail/Testcase.h"

#include "zypp/repo/DeltaCandidates.h"
#include "zypp/repo/PackageProvider.h"
#include "zypp/repo/SrcPackageProvider.h"

#include "zypp/sat/Pool.h"

#include "zypp/PluginScript.h"

using namespace std;


///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace target
  { /////////////////////////////////////////////////////////////////

    /** \internal Manage writing a new testcase when doing an upgrade. */
    void writeUpgradeTestcase()
    {
      unsigned toKeep( ZConfig::instance().solver_upgradeTestcasesToKeep() );
      MIL << "Testcases to keep: " << toKeep << endl;
      if ( !toKeep )
        return;
      Target_Ptr target( getZYpp()->getTarget() );
      if ( ! target )
      {
        WAR << "No Target no Testcase!" << endl;
        return;
      }

      std::string stem( "updateTestcase" );
      Pathname dir( target->assertRootPrefix("/var/log/") );
      Pathname next( dir / Date::now().form( stem+"-%Y-%m-%d-%H-%M-%S" ) );

      {
        std::list<std::string> content;
        filesystem::readdir( content, dir, /*dots*/false );
        std::set<std::string> cases;
        for_( c, content.begin(), content.end() )
        {
          if ( str::startsWith( *c, stem ) )
            cases.insert( *c );
        }
        if ( cases.size() >= toKeep )
        {
          unsigned toDel = cases.size() - toKeep + 1; // +1 for the new one
          for_( c, cases.begin(), cases.end() )
          {
            filesystem::recursive_rmdir( dir/(*c) );
            if ( ! --toDel )
              break;
          }
        }
      }

      MIL << "Write new testcase " << next << endl;
      getZYpp()->resolver()->createSolverTestcase( next.asString(), false/*no solving*/ );
    }

    ///////////////////////////////////////////////////////////////////
    namespace
    { /////////////////////////////////////////////////////////////////

      /** Execute script and report against report_r.
       * Return \c std::pair<bool,PatchScriptReport::Action> to indicate if
       * execution was successfull (<tt>first = true</tt>), or the desired
       * \c PatchScriptReport::Action in case execution failed
       * (<tt>first = false</tt>).
       *
       * \note The packager is responsible for setting the correct permissions
       * of the script. If the script is not executable it is reported as an
       * error. We must not modify the permessions.
       */
      std::pair<bool,PatchScriptReport::Action> doExecuteScript( const Pathname & root_r,
                                                                 const Pathname & script_r,
                                                                 callback::SendReport<PatchScriptReport> & report_r )
      {
        MIL << "Execute script " << PathInfo(Pathname::assertprefix( root_r,script_r)) << endl;

        HistoryLog historylog;
        historylog.comment(script_r.asString() + _(" executed"), /*timestamp*/true);
        ExternalProgram prog( script_r.asString(), ExternalProgram::Stderr_To_Stdout, false, -1, true, root_r );

        for ( std::string output = prog.receiveLine(); output.length(); output = prog.receiveLine() )
        {
          historylog.comment(output);
          if ( ! report_r->progress( PatchScriptReport::OUTPUT, output ) )
          {
            WAR << "User request to abort script " << script_r << endl;
            prog.kill();
            // the rest is handled by exit code evaluation
            // in case the script has meanwhile finished.
          }
        }

        std::pair<bool,PatchScriptReport::Action> ret( std::make_pair( false, PatchScriptReport::ABORT ) );

        if ( prog.close() != 0 )
        {
          ret.second = report_r->problem( prog.execError() );
          WAR << "ACTION" << ret.second << "(" << prog.execError() << ")" << endl;
          std::ostringstream sstr;
          sstr << script_r << _(" execution failed") << " (" << prog.execError() << ")" << endl;
          historylog.comment(sstr.str(), /*timestamp*/true);
          return ret;
        }

        report_r->finish();
        ret.first = true;
        return ret;
      }

      /** Execute script and report against report_r.
       * Return \c false if user requested \c ABORT.
       */
      bool executeScript( const Pathname & root_r,
                          const Pathname & script_r,
                          callback::SendReport<PatchScriptReport> & report_r )
      {
        std::pair<bool,PatchScriptReport::Action> action( std::make_pair( false, PatchScriptReport::ABORT ) );

        do {
          action = doExecuteScript( root_r, script_r, report_r );
          if ( action.first )
            return true; // success

          switch ( action.second )
          {
            case PatchScriptReport::ABORT:
              WAR << "User request to abort at script " << script_r << endl;
              return false; // requested abort.
              break;

            case PatchScriptReport::IGNORE:
              WAR << "User request to skip script " << script_r << endl;
              return true; // requested skip.
              break;

            case PatchScriptReport::RETRY:
              break; // again
          }
        } while ( action.second == PatchScriptReport::RETRY );

        // THIS is not intended to be reached:
        INT << "Abort on unknown ACTION request " << action.second << " returned" << endl;
        return false; // abort.
      }

      /** Look for update scripts named 'name-version-release-*' and
       *  execute them. Return \c false if \c ABORT was requested.
       *
       * \see http://en.opensuse.org/Software_Management/Code11/Scripts_and_Messages
       */
      bool RunUpdateScripts( const Pathname & root_r,
                             const Pathname & scriptsPath_r,
                             const std::vector<sat::Solvable> & checkPackages_r,
                             bool aborting_r )
      {
        if ( checkPackages_r.empty() )
          return true; // no installed packages to check

        MIL << "Looking for new update scripts in (" <<  root_r << ")" << scriptsPath_r << endl;
        Pathname scriptsDir( Pathname::assertprefix( root_r, scriptsPath_r ) );
        if ( ! PathInfo( scriptsDir ).isDir() )
          return true; // no script dir

        std::list<std::string> scripts;
        filesystem::readdir( scripts, scriptsDir, /*dots*/false );
        if ( scripts.empty() )
          return true; // no scripts in script dir

        // Now collect and execute all matching scripts.
        // On ABORT: at least log all outstanding scripts.
        // - "name-version-release"
        // - "name-version-release-*"
        bool abort = false;
        for_( it, checkPackages_r.begin(), checkPackages_r.end() )
        {
          std::string prefix( str::form( "%s-%s", it->name().c_str(), it->edition().c_str() ) );
          for_( sit, scripts.begin(), scripts.end() )
          {
            if ( ! str::hasPrefix( *sit, prefix ) )
              continue;

            if ( (*sit)[prefix.size()] != '\0' && (*sit)[prefix.size()] != '-' )
              continue; // if not exact match it had to continue with '-'

            PathInfo script( scriptsDir / *sit );
            if ( ! script.isFile() )
              continue;

            // Assert it's set executable
            filesystem::addmod( script.path(), 0500 );

            Pathname localPath( scriptsPath_r/(*sit) ); // without root prefix
            if ( abort || aborting_r )
            {
              WAR << "Aborting: Skip update script " << *sit << endl;
              HistoryLog().comment(
                  localPath.asString() + _(" execution skipped while aborting"),
                  /*timestamp*/true);
            }
            else
            {
              MIL << "Found update script " << *sit << endl;
              callback::SendReport<PatchScriptReport> report;
              report->start( make<Package>( *it ), script.path() );

              if ( ! executeScript( root_r, localPath, report ) ) // script path without root prefix!
                abort = true; // requested abort.
            }
          }
        }
        return !abort;
      }

      ///////////////////////////////////////////////////////////////////
      //
      ///////////////////////////////////////////////////////////////////

      inline void copyTo( std::ostream & out_r, const Pathname & file_r )
      {
        std::ifstream infile( file_r.c_str() );
        for( iostr::EachLine in( infile ); in; in.next() )
        {
          out_r << *in << endl;
        }
      }

      inline std::string notificationCmdSubst( const std::string & cmd_r, const UpdateNotificationFile & notification_r )
      {
        std::string ret( cmd_r );
#define SUBST_IF(PAT,VAL) if ( ret.find( PAT ) != std::string::npos ) ret = str::gsub( ret, PAT, VAL )
        SUBST_IF( "%p", notification_r.solvable().asString() );
        SUBST_IF( "%P", notification_r.file().asString() );
#undef SUBST_IF
        return ret;
      }

      void sendNotification( const Pathname & root_r,
                             const UpdateNotifications & notifications_r )
      {
        if ( notifications_r.empty() )
          return;

        std::string cmdspec( ZConfig::instance().updateMessagesNotify() );
        MIL << "Notification command is '" << cmdspec << "'" << endl;
        if ( cmdspec.empty() )
          return;

        std::string::size_type pos( cmdspec.find( '|' ) );
        if ( pos == std::string::npos )
        {
          ERR << "Can't send Notification: Missing 'format |' in command spec." << endl;
          HistoryLog().comment( str::Str() << _("Error sending update message notification."), /*timestamp*/true );
          return;
        }

        std::string formatStr( str::toLower( str::trim( cmdspec.substr( 0, pos ) ) ) );
        std::string commandStr( str::trim( cmdspec.substr( pos + 1 ) ) );

        enum Format { UNKNOWN, NONE, SINGLE, DIGEST, BULK };
        Format format = UNKNOWN;
        if ( formatStr == "none" )
          format = NONE;
        else if ( formatStr == "single" )
          format = SINGLE;
        else if ( formatStr == "digest" )
          format = DIGEST;
        else if ( formatStr == "bulk" )
          format = BULK;
        else
        {
          ERR << "Can't send Notification: Unknown format '" << formatStr << " |' in command spec." << endl;
          HistoryLog().comment( str::Str() << _("Error sending update message notification."), /*timestamp*/true );
         return;
        }

        // Take care: commands are ececuted chroot(root_r). The message file
        // pathnames in notifications_r are local to root_r. For physical access
        // to the file they need to be prefixed.

        if ( format == NONE || format == SINGLE )
        {
          for_( it, notifications_r.begin(), notifications_r.end() )
          {
            std::vector<std::string> command;
            if ( format == SINGLE )
              command.push_back( "<"+Pathname::assertprefix( root_r, it->file() ).asString() );
            str::splitEscaped( notificationCmdSubst( commandStr, *it ), std::back_inserter( command ) );

            ExternalProgram prog( command, ExternalProgram::Stderr_To_Stdout, false, -1, true, root_r );
            if ( true ) // Wait for feedback
            {
              for( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() )
              {
                DBG << line;
              }
              int ret = prog.close();
              if ( ret != 0 )
              {
                ERR << "Notification command returned with error (" << ret << ")." << endl;
                HistoryLog().comment( str::Str() << _("Error sending update message notification."), /*timestamp*/true );
                return;
              }
            }
          }
        }
        else if ( format == DIGEST || format == BULK )
        {
          filesystem::TmpFile tmpfile;
          ofstream out( tmpfile.path().c_str() );
          for_( it, notifications_r.begin(), notifications_r.end() )
          {
            if ( format == DIGEST )
            {
              out << it->file() << endl;
            }
            else if ( format == BULK )
            {
              copyTo( out << '\f', Pathname::assertprefix( root_r, it->file() ) );
            }
          }

          std::vector<std::string> command;
          command.push_back( "<"+tmpfile.path().asString() ); // redirect input
          str::splitEscaped( notificationCmdSubst( commandStr, *notifications_r.begin() ), std::back_inserter( command ) );

          ExternalProgram prog( command, ExternalProgram::Stderr_To_Stdout, false, -1, true, root_r );
          if ( true ) // Wait for feedback otherwise the TmpFile goes out of scope.
          {
            for( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() )
            {
              DBG << line;
            }
            int ret = prog.close();
            if ( ret != 0 )
            {
              ERR << "Notification command returned with error (" << ret << ")." << endl;
              HistoryLog().comment( str::Str() << _("Error sending update message notification."), /*timestamp*/true );
              return;
            }
          }
        }
        else
        {
          INT << "Can't send Notification: Missing handler for 'format |' in command spec." << endl;
          HistoryLog().comment( str::Str() << _("Error sending update message notification."), /*timestamp*/true );
          return;
        }
      }


      /** Look for update messages named 'name-version-release-*' and
       *  send notification according to \ref ZConfig::updateMessagesNotify.
       *
       * \see http://en.opensuse.org/Software_Management/Code11/Scripts_and_Messages
       */
      void RunUpdateMessages( const Pathname & root_r,
                              const Pathname & messagesPath_r,
                              const std::vector<sat::Solvable> & checkPackages_r,
                              ZYppCommitResult & result_r )
      {
        if ( checkPackages_r.empty() )
          return; // no installed packages to check

        MIL << "Looking for new update messages in (" <<  root_r << ")" << messagesPath_r << endl;
        Pathname messagesDir( Pathname::assertprefix( root_r, messagesPath_r ) );
        if ( ! PathInfo( messagesDir ).isDir() )
          return; // no messages dir

        std::list<std::string> messages;
        filesystem::readdir( messages, messagesDir, /*dots*/false );
        if ( messages.empty() )
          return; // no messages in message dir

        // Now collect all matching messages in result and send them
        // - "name-version-release"
        // - "name-version-release-*"
        HistoryLog historylog;
        for_( it, checkPackages_r.begin(), checkPackages_r.end() )
        {
          std::string prefix( str::form( "%s-%s", it->name().c_str(), it->edition().c_str() ) );
          for_( sit, messages.begin(), messages.end() )
          {
            if ( ! str::hasPrefix( *sit, prefix ) )
              continue;

            if ( (*sit)[prefix.size()] != '\0' && (*sit)[prefix.size()] != '-' )
              continue; // if not exact match it had to continue with '-'

            PathInfo message( messagesDir / *sit );
            if ( ! message.isFile() || message.size() == 0 )
              continue;

            MIL << "Found update message " << *sit << endl;
            Pathname localPath( messagesPath_r/(*sit) ); // without root prefix
            result_r.setUpdateMessages().push_back( UpdateNotificationFile( *it, localPath ) );
            historylog.comment( str::Str() << _("New update message") << " " << localPath, /*timestamp*/true );
          }
        }
        sendNotification( root_r, result_r.updateMessages() );
      }

      /////////////////////////////////////////////////////////////////
    } // namespace
    ///////////////////////////////////////////////////////////////////

    void XRunUpdateMessages( const Pathname & root_r,
                             const Pathname & messagesPath_r,
                             const std::vector<sat::Solvable> & checkPackages_r,
                             ZYppCommitResult & result_r )
    { RunUpdateMessages( root_r, messagesPath_r, checkPackages_r, result_r ); }

    /** Helper for PackageProvider queries during commit. */
    struct QueryInstalledEditionHelper
    {
      bool operator()( const std::string & name_r,
                       const Edition &     ed_r,
                       const Arch &        arch_r ) const
      {
        rpm::librpmDb::db_const_iterator it;
        for ( it.findByName( name_r ); *it; ++it )
          {
            if ( arch_r == it->tag_arch()
                 && ( ed_r == Edition::noedition || ed_r == it->tag_edition() ) )
              {
                return true;
              }
          }
        return false;
      }
    };

    /**
     * \short Let the Source provide the package.
     * \p pool_r \ref ResPool used to get candidates
     * \p pi item to be commited
    */
    struct RepoProvidePackage
    {
      ResPool _pool;
      repo::RepoMediaAccess &_access;

      RepoProvidePackage( repo::RepoMediaAccess &access, ResPool pool_r )
        : _pool(pool_r), _access(access)
      {}

      ManagedFile operator()( const PoolItem & pi )
      {
        // Redirect PackageProvider queries for installed editions
        // (in case of patch/delta rpm processing) to rpmDb.
        repo::PackageProviderPolicy packageProviderPolicy;
        packageProviderPolicy.queryInstalledCB( QueryInstalledEditionHelper() );

        Package::constPtr p = asKind<Package>(pi.resolvable());

        // Build a repository list for repos
        // contributing to the pool
        std::list<Repository> repos( _pool.knownRepositoriesBegin(), _pool.knownRepositoriesEnd() );
        repo::DeltaCandidates deltas(repos, p->name());
        repo::PackageProvider pkgProvider( _access, p, deltas, packageProviderPolicy );

        ManagedFile ret( pkgProvider.providePackage() );
        return ret;
      }
    };
    ///////////////////////////////////////////////////////////////////

    IMPL_PTR_TYPE(TargetImpl);

    TargetImpl_Ptr TargetImpl::_nullimpl;

    /** Null implementation */
    TargetImpl_Ptr TargetImpl::nullimpl()
    {
      if (_nullimpl == 0)
        _nullimpl = new TargetImpl;
      return _nullimpl;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : TargetImpl::TargetImpl
    //	METHOD TYPE : Ctor
    //
    TargetImpl::TargetImpl( const Pathname & root_r, bool doRebuild_r )
    : _root( root_r )
    , _requestedLocalesFile( home() / "RequestedLocales" )
    , _softLocksFile( home() / "SoftLocks" )
    , _hardLocksFile( Pathname::assertprefix( _root, ZConfig::instance().locksFile() ) )
    {
      _rpm.initDatabase( root_r, Pathname(), doRebuild_r );

      HistoryLog::setRoot(_root);

      createAnonymousId();

      MIL << "Initialized target on " << _root << endl;
    }

    /**
     * generates a random id using uuidgen
     */
    static string generateRandomId()
    {
      string id;
      const char* argv[] =
      {
         "/usr/bin/uuidgen",
         NULL
      };

      ExternalProgram prog( argv,
                            ExternalProgram::Normal_Stderr,
                            false, -1, true);
      std::string line;
      for(line = prog.receiveLine();
          ! line.empty();
          line = prog.receiveLine() )
      {
          MIL << line << endl;
          id = line;
          break;
      }
      prog.close();
      return id;
    }

    /**
     * updates the content of \p filename
     * if \p condition is true, setting the content
     * the the value returned by \p value
     */
    void updateFileContent( const Pathname &filename,
                            boost::function<bool ()> condition,
                            boost::function<string ()> value )
    {
        string val = value();
        // if the value is empty, then just dont
        // do anything, regardless of the condition
        if ( val.empty() )
            return;

        if ( condition() )
        {
            MIL << "updating '" << filename << "' content." << endl;

            // if the file does not exist we need to generate the uuid file

            std::ofstream filestr;
            // make sure the path exists
            filesystem::assert_dir( filename.dirname() );
            filestr.open( filename.c_str() );

            if ( filestr.good() )
            {
                filestr << val;
                filestr.close();
            }
            else
            {
                // FIXME, should we ignore the error?
                ZYPP_THROW(Exception("Can't openfile '" + filename.asString() + "' for writing"));
            }
        }
    }

    /** helper functor */
    static bool fileMissing( const Pathname &pathname )
    {
        return ! PathInfo(pathname).isExist();
    }

    void TargetImpl::createAnonymousId() const
    {

      // create the anonymous unique id
      // this value is used for statistics
      Pathname idpath( home() / "AnonymousUniqueId");

      try
      {
        updateFileContent( idpath,
                           boost::bind(fileMissing, idpath),
                           generateRandomId );
      }
      catch ( const Exception &e )
      {
        WAR << "Can't create anonymous id file" << endl;
      }

    }

    void TargetImpl::createLastDistributionFlavorCache() const
    {
      // create the anonymous unique id
      // this value is used for statistics
      Pathname flavorpath( home() / "LastDistributionFlavor");

      // is there a product
      Product::constPtr p = baseProduct();
      if ( ! p )
      {
          WAR << "No base product, I won't create flavor cache" << endl;
          return;
      }

      string flavor = p->flavor();

      try
      {

        updateFileContent( flavorpath,
                           // only if flavor is not empty
                           functor::Constant<bool>( ! flavor.empty() ),
                           functor::Constant<string>(flavor) );
      }
      catch ( const Exception &e )
      {
        WAR << "Can't create flavor cache" << endl;
        return;
      }
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : TargetImpl::~TargetImpl
    //	METHOD TYPE : Dtor
    //
    TargetImpl::~TargetImpl()
    {
      _rpm.closeDatabase();
      MIL << "Targets closed" << endl;
    }

    ///////////////////////////////////////////////////////////////////
    //
    // solv file handling
    //
    ///////////////////////////////////////////////////////////////////

    Pathname TargetImpl::defaultSolvfilesPath() const
    {
      return Pathname::assertprefix( _root, ZConfig::instance().repoSolvfilesPath() / sat::Pool::instance().systemRepoAlias() );
    }

    void TargetImpl::clearCache()
    {
      Pathname base = solvfilesPath();
      filesystem::recursive_rmdir( base );
    }

    void TargetImpl::buildCache()
    {
      Pathname base = solvfilesPath();
      Pathname rpmsolv       = base/"solv";
      Pathname rpmsolvcookie = base/"cookie";

      bool build_rpm_solv = true;
      // lets see if the rpm solv cache exists

      RepoStatus rpmstatus( RepoStatus( _root/"/var/lib/rpm/Name" )
                            && (_root/"/etc/products.d") );

      bool solvexisted = PathInfo(rpmsolv).isExist();
      if ( solvexisted )
      {
        // see the status of the cache
        PathInfo cookie( rpmsolvcookie );
        MIL << "Read cookie: " << cookie << endl;
        if ( cookie.isExist() )
        {
          RepoStatus status = RepoStatus::fromCookieFile(rpmsolvcookie);
          // now compare it with the rpm database
          if ( status.checksum() == rpmstatus.checksum() )
            build_rpm_solv = false;
          MIL << "Read cookie: " << rpmsolvcookie << " says: "
              << (build_rpm_solv ? "outdated" : "uptodate") << endl;
        }
      }

      if ( build_rpm_solv )
      {
        // if the solvfile dir does not exist yet, we better create it
        filesystem::assert_dir( base );

        Pathname oldSolvFile( solvexisted ? rpmsolv : Pathname() ); // to speedup rpmdb2solv

        filesystem::TmpFile tmpsolv( filesystem::TmpFile::makeSibling( rpmsolv ) );
        if ( !tmpsolv )
        {
          // Can't create temporary solv file, usually due to insufficient permission
          // (user query while @System solv needs refresh). If so, try switching
          // to a location within zypps temp. space (will be cleaned at application end).

          bool switchingToTmpSolvfile = false;
          Exception ex("Failed to cache rpm database.");
          ex.remember(str::form("Cannot create temporary file under %s.", base.c_str()));

          if ( ! solvfilesPathIsTemp() )
          {
            base = getZYpp()->tmpPath() / sat::Pool::instance().systemRepoAlias();
            rpmsolv       = base/"solv";
            rpmsolvcookie = base/"cookie";

            filesystem::assert_dir( base );
            tmpsolv = filesystem::TmpFile::makeSibling( rpmsolv );

            if ( tmpsolv )
            {
              WAR << "Using a temporary solv file at " << base << endl;
              switchingToTmpSolvfile = true;
              _tmpSolvfilesPath = base;
            }
            else
            {
              ex.remember(str::form("Cannot create temporary file under %s.", base.c_str()));
            }
          }

          if ( ! switchingToTmpSolvfile )
          {
            ZYPP_THROW(ex);
          }
        }

        // Take care we unlink the solvfile on exception
        ManagedFile guard( base, filesystem::recursive_rmdir );

        std::ostringstream cmd;
        cmd << "rpmdb2solv";
        if ( ! _root.empty() )
          cmd << " -r '" << _root << "'";

        cmd << " -p '" << Pathname::assertprefix( _root, "/etc/products.d" ) << "'";

        if ( ! oldSolvFile.empty() )
          cmd << " '" << oldSolvFile << "'";

        cmd << "  > '" << tmpsolv.path() << "'";

        MIL << "Executing: " << cmd << endl;
        ExternalProgram prog( cmd.str(), ExternalProgram::Stderr_To_Stdout );

        cmd << endl;
        for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() ) {
          WAR << "  " << output;
          cmd << "     " << output;
        }

        int ret = prog.close();
        if ( ret != 0 )
        {
          Exception ex(str::form("Failed to cache rpm database (%d).", ret));
          ex.remember( cmd.str() );
          ZYPP_THROW(ex);
        }

        ret = filesystem::rename( tmpsolv, rpmsolv );
        if ( ret != 0 )
          ZYPP_THROW(Exception("Failed to move cache to final destination"));
        // if this fails, don't bother throwing exceptions
        filesystem::chmod( rpmsolv, 0644 );

        rpmstatus.saveToCookieFile(rpmsolvcookie);

        // We keep it.
        guard.resetDispose();

	// Finally send notification to plugins
	// NOTE: quick hack looking for spacewalk plugin only
	{
	  Pathname script( Pathname::assertprefix( _root, ZConfig::instance().pluginsPath()/"system/spacewalk" ) );
	  if ( PathInfo( script ).isX() )
	    try {
	      PluginScript spacewalk( script );
	      spacewalk.open();

	      PluginFrame notify( "PACKAGESETCHANGED" );
	      spacewalk.send( notify );

	      PluginFrame ret( spacewalk.receive() );
	      MIL << ret << endl;
	      if ( ret.command() == "ERROR" )
		ret.writeTo( WAR ) << endl;
	    }
	    catch ( const Exception & excpt )
	    {
	      WAR << excpt.asUserHistory() << endl;
	    }
	}
      }
    }

    void TargetImpl::unload()
    {
      Repository system( sat::Pool::instance().findSystemRepo() );
      if ( system )
        system.eraseFromPool();
    }


    void TargetImpl::load()
    {
      buildCache();

      // now add the repos to the pool
      sat::Pool satpool( sat::Pool::instance() );
      Pathname rpmsolv( solvfilesPath() / "solv" );
      MIL << "adding " << rpmsolv << " to pool(" << satpool.systemRepoAlias() << ")" << endl;

      // Providing an empty system repo, unload any old content
      Repository system( sat::Pool::instance().findSystemRepo() );
      if ( system && ! system.solvablesEmpty() )
      {
        system.eraseFromPool(); // invalidates system
      }
      if ( ! system )
      {
        system = satpool.systemRepo();
      }

      try
      {
        system.addSolv( rpmsolv );
      }
      catch ( const Exception & exp )
      {
        ZYPP_CAUGHT( exp );
        MIL << "Try to handle exception by rebuilding the solv-file" << endl;
        clearCache();
        buildCache();

        system.addSolv( rpmsolv );
      }

      // (Re)Load the requested locales et al.
      // If the requested locales are empty, we leave the pool untouched
      // to avoid undoing changes the application applied. We expect this
      // to happen on a bare metal installation only. An already existing
      // target should be loaded before its settings are changed.
      {
        const LocaleSet & requestedLocales( _requestedLocalesFile.locales() );
        if ( ! requestedLocales.empty() )
        {
          satpool.setRequestedLocales( requestedLocales );
        }
      }
      {
        SoftLocksFile::Data softLocks( _softLocksFile.data() );
        if ( ! softLocks.empty() )
        {
          // Don't soft lock any installed item.
          for_( it, system.solvablesBegin(), system.solvablesEnd() )
          {
            softLocks.erase( it->ident() );
          }
          ResPool::instance().setAutoSoftLocks( softLocks );
        }
      }
      if ( ZConfig::instance().apply_locks_file() )
      {
        const HardLocksFile::Data & hardLocks( _hardLocksFile.data() );
        if ( ! hardLocks.empty() )
        {
          ResPool::instance().setHardLockQueries( hardLocks );
        }
      }

      // now that the target is loaded, we can cache the flavor
      createLastDistributionFlavorCache();

      MIL << "Target loaded: " << system.solvablesSize() << " resolvables" << endl;
    }

    ///////////////////////////////////////////////////////////////////
    //
    // COMMIT
    //
    ///////////////////////////////////////////////////////////////////
    ZYppCommitResult TargetImpl::commit( ResPool pool_r, const ZYppCommitPolicy & policy_rX )
    {
      // ----------------------------------------------------------------- //
      ZYppCommitPolicy policy_r( policy_rX );

      // Fake outstanding YCP fix: Honour restriction to media 1
      // at installation, but install all remaining packages if post-boot.
      if ( policy_r.restrictToMedia() > 1 )
        policy_r.allMedia();

      // DownloadOnly implies dry-run.
      if ( policy_r.downloadMode() == DownloadOnly )
        policy_r.dryRun( true );
      // ----------------------------------------------------------------- //

      MIL << "TargetImpl::commit(<pool>, " << policy_r << ")" << endl;

      ///////////////////////////////////////////////////////////////////
      // Write out a testcase if we're in dist upgrade mode.
      ///////////////////////////////////////////////////////////////////
      if ( getZYpp()->resolver()->upgradeMode() )
      {
        if ( ! policy_r.dryRun() )
        {
          writeUpgradeTestcase();
        }
        else
        {
          DBG << "dryRun: Not writing upgrade testcase." << endl;
        }
      }

      ///////////////////////////////////////////////////////////////////
      // Store non-package data:
      ///////////////////////////////////////////////////////////////////
      if ( ! policy_r.dryRun() )
      {
        filesystem::assert_dir( home() );
        // requested locales
        _requestedLocalesFile.setLocales( pool_r.getRequestedLocales() );
        // weak locks
        {
          SoftLocksFile::Data newdata;
          pool_r.getActiveSoftLocks( newdata );
          _softLocksFile.setData( newdata );
        }
        // hard locks
        if ( ZConfig::instance().apply_locks_file() )
        {
          HardLocksFile::Data newdata;
          pool_r.getHardLockQueries( newdata );
          _hardLocksFile.setData( newdata );
        }
      }
      else
      {
        DBG << "dryRun: Not stroring non-package data." << endl;
      }

      ///////////////////////////////////////////////////////////////////
      // Compute transaction:
      ///////////////////////////////////////////////////////////////////

      ZYppCommitResult result( root() );
      TargetImpl::PoolItemList to_uninstall;
      TargetImpl::PoolItemList to_install;
      TargetImpl::PoolItemList to_srcinstall;

      {
        pool::GetResolvablesToInsDel
        collect( pool_r, policy_r.restrictToMedia() ? pool::GetResolvablesToInsDel::ORDER_BY_MEDIANR
                 : pool::GetResolvablesToInsDel::ORDER_BY_SOURCE );
        MIL << "GetResolvablesToInsDel: " << endl << collect << endl;
        to_uninstall.swap ( collect._toDelete );
        to_install.swap   ( collect._toInstall );
        to_srcinstall.swap( collect._toSrcinstall );
      }

      if ( policy_r.restrictToMedia() )
      {
        MIL << "Restrict to media number " << policy_r.restrictToMedia() << endl;

        TargetImpl::PoolItemList current_install;
        TargetImpl::PoolItemList current_srcinstall;

        // Collect until the 1st package from an unwanted media occurs.
        // Further collection could violate install order.
        bool hitUnwantedMedia = false;
        for ( TargetImpl::PoolItemList::iterator it = to_install.begin(); it != to_install.end(); ++it )
        {
          ResObject::constPtr res( it->resolvable() );

          if ( hitUnwantedMedia
               || ( res->mediaNr() && res->mediaNr() != policy_r.restrictToMedia() ) )
          {
            hitUnwantedMedia = true;
            result._remaining.push_back( *it );
          }
          else
          {
            current_install.push_back( *it );
          }
        }

        for (TargetImpl::PoolItemList::iterator it = to_srcinstall.begin(); it != to_srcinstall.end(); ++it)
        {
          Resolvable::constPtr res( it->resolvable() );
          Package::constPtr pkg( asKind<Package>(res) );
          if ( pkg && policy_r.restrictToMedia() != pkg->mediaNr() ) // check medianr for packages only
          {
            result._srcremaining.push_back( *it );
          }
          else
          {
            current_srcinstall.push_back( *it );
          }
        }

        to_install.swap   ( current_install );
        to_srcinstall.swap( current_srcinstall );
      }

      ///////////////////////////////////////////////////////////////////
      // First collect and display all messages
      // associated with patches to be installed.
      ///////////////////////////////////////////////////////////////////
      if ( ! policy_r.dryRun() )
      {
        for_( it, to_install.begin(), to_install.end() )
        {
          if ( ! isKind<Patch>(it->resolvable()) )
            continue;
          if ( ! it->status().isToBeInstalled() )
            continue;

          Patch::constPtr patch( asKind<Patch>(it->resolvable()) );
          if ( ! patch->message().empty() )
          {
            MIL << "Show message for " << patch << endl;
            callback::SendReport<target::PatchMessageReport> report;
            if ( ! report->show( patch ) )
            {
              WAR << "commit aborted by the user" << endl;
              ZYPP_THROW( TargetAbortedException( N_("Installation has been aborted as directed.") ) );
            }
          }
        }
      }
      else
      {
        DBG << "dryRun: Not checking patch messages." << endl;
      }

      ///////////////////////////////////////////////////////////////////
      // Remove/install packages.
      ///////////////////////////////////////////////////////////////////

      DBG << "commit log file is set to: " << HistoryLog::fname() << endl;
      if ( ! policy_r.dryRun() || policy_r.downloadMode() == DownloadOnly )
      {
        // somewhat uggly constraint: The iterator passed to the CommitPackageCache
        // must match begin and end of the install PoolItemList passed to commit.
        // For the download policies it's the easiest, if we have just a single
        // toInstall list. So we unify to_install and to_srcinstall. In case
        // of errors we have to split it up again. This will be cleaned up when
        // we introduce the new install order.
        TargetImpl::PoolItemList items( to_install );
        items.insert( items.end(), to_srcinstall.begin(), to_srcinstall.end() );

        // prepare the package cache according to the download options:
        repo::RepoMediaAccess access;
        RepoProvidePackage repoProvidePackage( access, pool_r );
        CommitPackageCache packageCache( items.begin(), items.end(),
                                         root() / "tmp", repoProvidePackage );

        bool miss = false;
        if ( policy_r.downloadMode() != DownloadAsNeeded )
        {
          // Preload the cache. Until now this means pre-loading all packages.
          // Once DownloadInHeaps is fully implemented, this will change and
          // we may actually have more than one heap.
          for_( it, items.begin(), items.end() )
          {
            if ( (*it)->isKind<Package>() || (*it)->isKind<SrcPackage>() )
            {
              ManagedFile localfile;
              try
              {
		if ( (*it)->isKind<Package>() )
		{
		  localfile = packageCache.get( it );
		}
		else if ( (*it)->isKind<SrcPackage>() )
		{
		  repo::RepoMediaAccess access;
		  repo::SrcPackageProvider prov( access );
		  localfile = prov.provideSrcPackage( (*it)->asKind<SrcPackage>() );
		}
		else
		{
		  INT << "Don't know howto cache: Neither Package nor SrcPackage: " << *it << endl;
		  continue;
		}
                localfile.resetDispose(); // keep the package file in the cache
              }
              catch ( const AbortRequestException & exp )
              {
                miss = true;
		result._errors.push_back( *it );
                WAR << "commit cache preload aborted by the user" << endl;
                ZYPP_THROW( TargetAbortedException( N_("Installation has been aborted as directed.") ) );
                break;
              }
              catch ( const SkipRequestException & exp )
              {
                ZYPP_CAUGHT( exp );
                miss = true;
		result._errors.push_back( *it );
                WAR << "Skipping cache preload package " << (*it)->asKind<Package>() << " in commit" << endl;
                continue;
              }
              catch ( const Exception & exp )
              {
                // bnc #395704: missing catch causes abort.
                // TODO see if packageCache fails to handle errors correctly.
                ZYPP_CAUGHT( exp );
                miss = true;
		result._errors.push_back( *it );
                INT << "Unexpected Error: Skipping cache preload package " << (*it)->asKind<Package>() << " in commit" << endl;
                continue;
              }
            }
          }
        }

        if ( miss )
        {
          ERR << "Some packages could not be provided. Aborting commit."<< endl;
          result._remaining.insert( result._remaining.end(), to_install.begin(), to_install.end() );
          result._srcremaining.insert( result._srcremaining.end(), to_srcinstall.begin(), to_srcinstall.end() );
        }
        else if ( ! policy_r.dryRun() )
        {
          commit ( to_uninstall, policy_r, result, packageCache );
          TargetImpl::PoolItemList bad = commit( items, policy_r, result, packageCache );
          if ( ! bad.empty() )
          {
            for_( it, bad.begin(), bad.end() )
            {
              if ( isKind<SrcPackage>(it->resolvable()) )
                result._srcremaining.push_back( *it );
              else
                result._remaining.push_back( *it );
            }
          }
        }
        else
        {
          DBG << "dryRun: Not installing/deleting anything." << endl;
        }
      }
      else
      {
        DBG << "dryRun: Not downloading/installing/deleting anything." << endl;
      }

      ///////////////////////////////////////////////////////////////////
      // Try to rebuild solv file while rpm database is still in cache
      ///////////////////////////////////////////////////////////////////
      if ( ! policy_r.dryRun() )
      {
        buildCache();
      }

      result._result = (to_install.size() - result._remaining.size());
      MIL << "TargetImpl::commit(<pool>, " << policy_r << ") returns: " << result << endl;
      return result;
    }


    ///////////////////////////////////////////////////////////////////
    //
    // COMMIT internal
    //
    ///////////////////////////////////////////////////////////////////
    TargetImpl::PoolItemList
    TargetImpl::commit( const TargetImpl::PoolItemList & items_r,
                        const ZYppCommitPolicy & policy_r,
                        ZYppCommitResult & result_r,
                        CommitPackageCache & packageCache_r )
    {
      MIL << "TargetImpl::commit(<list>" << policy_r << ")" << items_r.size() << endl;

      bool abort = false;
      std::vector<sat::Solvable> successfullyInstalledPackages;
      TargetImpl::PoolItemList remaining;

      for ( TargetImpl::PoolItemList::const_iterator it = items_r.begin(); it != items_r.end(); it++ )
      {
        if ( (*it)->isKind<Package>() )
        {
          Package::constPtr p = (*it)->asKind<Package>();
          if (it->status().isToBeInstalled())
          {
            ManagedFile localfile;
            try
            {
              localfile = packageCache_r.get( it );
            }
            catch ( const AbortRequestException &e )
            {
              WAR << "commit aborted by the user" << endl;
              abort = true;
	      result_r._errors.push_back( *it );
	      remaining.insert( remaining.end(), it, items_r.end() );
	      break;
            }
            catch ( const SkipRequestException &e )
            {
              ZYPP_CAUGHT( e );
              WAR << "Skipping package " << p << " in commit" << endl;
	      result_r._errors.push_back( *it );
	      remaining.push_back( *it );
              continue;
            }
            catch ( const Exception &e )
            {
              // bnc #395704: missing catch causes abort.
              // TODO see if packageCache fails to handle errors correctly.
              ZYPP_CAUGHT( e );
              INT << "Unexpected Error: Skipping package " << p << " in commit" << endl;
	      result_r._errors.push_back( *it );
	      remaining.push_back( *it );
              continue;
            }

#warning Exception handling
            // create a installation progress report proxy
            RpmInstallPackageReceiver progress( it->resolvable() );
            progress.connect(); // disconnected on destruction.

            bool success = false;
            rpm::RpmInstFlags flags( policy_r.rpmInstFlags() & rpm::RPMINST_JUSTDB );
            // Why force and nodeps?
            //
            // Because zypp builds the transaction and the resolver asserts that
            // everything is fine.
            // We use rpm just to unpack and register the package in the database.
            // We do this step by step, so rpm is not aware of the bigger context.
            // So we turn off rpms internal checks, because we do it inside zypp.
            flags |= rpm::RPMINST_NODEPS;
            flags |= rpm::RPMINST_FORCE;
            //
            if (p->multiversionInstall())  flags |= rpm::RPMINST_NOUPGRADE;
            if (policy_r.dryRun())         flags |= rpm::RPMINST_TEST;
            if (policy_r.rpmExcludeDocs()) flags |= rpm::RPMINST_EXCLUDEDOCS;
            if (policy_r.rpmNoSignature()) flags |= rpm::RPMINST_NOSIGNATURE;

            try
            {
              progress.tryLevel( target::rpm::InstallResolvableReport::RPM_NODEPS_FORCE );
              rpm().installPackage( localfile, flags );
              HistoryLog().install(*it);

              if ( progress.aborted() )
              {
                WAR << "commit aborted by the user" << endl;
                localfile.resetDispose(); // keep the package file in the cache
                abort = true;
		result_r._errors.push_back( *it );
		remaining.insert( remaining.end(), it, items_r.end() );
                break;
              }
              else
              {
                success = true;
              }
            }
            catch ( Exception & excpt_r )
            {
              ZYPP_CAUGHT(excpt_r);
              localfile.resetDispose(); // keep the package file in the cache

              if ( policy_r.dryRun() )
              {
                WAR << "dry run failed" << endl;
		result_r._errors.push_back( *it );
		remaining.insert( remaining.end(), it, items_r.end() );
                break;
              }
              // else
              if ( progress.aborted() )
              {
                WAR << "commit aborted by the user" << endl;
                abort = true;
              }
              else
              {
                WAR << "Install failed" << endl;
              }
	      result_r._errors.push_back( *it );
	      remaining.insert( remaining.end(), it, items_r.end() );
              break; // stop
            }

            if ( success && !policy_r.dryRun() )
            {
              it->status().resetTransact( ResStatus::USER );
              // Remember to check this package for presence of patch scripts.
              successfullyInstalledPackages.push_back( it->satSolvable() );
            }
          }
          else
          {
            RpmRemovePackageReceiver progress( it->resolvable() );
            progress.connect(); // disconnected on destruction.

            bool success = false;
            rpm::RpmInstFlags flags( policy_r.rpmInstFlags() & rpm::RPMINST_JUSTDB );
            flags |= rpm::RPMINST_NODEPS;
            if (policy_r.dryRun()) flags |= rpm::RPMINST_TEST;
            try
            {
              rpm().removePackage( p, flags );
              HistoryLog().remove(*it);

              if ( progress.aborted() )
              {
                WAR << "commit aborted by the user" << endl;
                abort = true;
		result_r._errors.push_back( *it );
                break;
              }
              else
              {
                success = true;
              }
            }
            catch (Exception & excpt_r)
            {
              ZYPP_CAUGHT( excpt_r );
              if ( progress.aborted() )
              {
                WAR << "commit aborted by the user" << endl;
                abort = true;
		result_r._errors.push_back( *it );
                break;
              }
              // else
              WAR << "removal of " << p << " failed";
	      result_r._errors.push_back( *it );
            }
            if ( success && !policy_r.dryRun() )
            {
              it->status().resetTransact( ResStatus::USER );
            }
          }
        }
        else if ( ! policy_r.dryRun() ) // other resolvables (non-Package)
        {
          // Status is changed as the buddy package buddy
          // gets installed/deleted. Handle non-buddies only.
          if ( ! it->buddy() )
          {
            if ( (*it)->isKind<Product>() )
            {
              Product::constPtr p = (*it)->asKind<Product>();
              if ( it->status().isToBeInstalled() )
              {
                ERR << "Can't install orphan product without release-package! " << (*it) << endl;
              }
              else
              {
                // Deleting the corresponding product entry is all we con do.
                // So the product will no longer be visible as installed.
                std::string referenceFilename( p->referenceFilename() );
                if ( referenceFilename.empty() )
                {
                  ERR << "Can't remove orphan product without 'referenceFilename'! " << (*it) << endl;
                }
                else
                {
                  PathInfo referenceFile( Pathname::assertprefix( _root, Pathname( "/etc/products.d" ) ) / referenceFilename );
                  if ( ! referenceFile.isFile() || filesystem::unlink( referenceFile.path() ) != 0 )
                  {
                    ERR << "Delete orphan product failed: " << referenceFile << endl;
                  }
                }
              }
            }
            else if ( (*it)->isKind<SrcPackage>() && it->status().isToBeInstalled() )
            {
              // SrcPackage is install-only
              SrcPackage::constPtr p = (*it)->asKind<SrcPackage>();
              installSrcPackage( p );
            }

            it->status().resetTransact( ResStatus::USER );
          }
        }  // other resolvables

      } // for

      // Check presence of update scripts/messages. If aborting,
      // at least log omitted scripts.
      if ( ! successfullyInstalledPackages.empty() )
      {
        if ( ! RunUpdateScripts( _root, ZConfig::instance().update_scriptsPath(),
                                 successfullyInstalledPackages, abort ) )
        {
          WAR << "Commit aborted by the user" << endl;
          abort = true;
        }
        // send messages after scripts in case some script generates output,
        // that should be kept in t %ghost message file.
        RunUpdateMessages( _root, ZConfig::instance().update_messagesPath(),
                           successfullyInstalledPackages,
                           result_r );
      }

      if ( abort )
      {
        ZYPP_THROW( TargetAbortedException( N_("Installation has been aborted as directed.") ) );
      }

     return remaining;
    }

    rpm::RpmDb & TargetImpl::rpm()
    {
      return _rpm;
    }

    bool TargetImpl::providesFile (const std::string & path_str, const std::string & name_str) const
    {
      return _rpm.hasFile(path_str, name_str);
    }


    Date TargetImpl::timestamp() const
    {
      return _rpm.timestamp();
    }

    ///////////////////////////////////////////////////////////////////

    Product::constPtr TargetImpl::baseProduct() const
    {
      ResPool pool(ResPool::instance());
      for_( it, pool.byKindBegin<Product>(), pool.byKindEnd<Product>() )
      {
        Product::constPtr p = (*it)->asKind<Product>();
        if ( p->isTargetDistribution() )
          return p;
      }
      return 0L;
    }

    ///////////////////////////////////////////////////////////////////

    namespace
    {
      parser::ProductFileData baseproductdata( const Pathname & root_r )
      {
        PathInfo baseproduct( Pathname::assertprefix( root_r, "/etc/products.d/baseproduct" ) );
        if ( baseproduct.isFile() )
        {
          try
          {
            return parser::ProductFileReader::scanFile( baseproduct.path() );
          }
          catch ( const Exception & excpt )
          {
            ZYPP_CAUGHT( excpt );
          }
        }
        return parser::ProductFileData();
      }

      inline Pathname staticGuessRoot( const Pathname & root_r )
      {
        if ( root_r.empty() )
        {
          // empty root: use existing Target or assume "/"
          Pathname ret ( ZConfig::instance().systemRoot() );
          if ( ret.empty() )
            return Pathname("/");
          return ret;
        }
        return root_r;
      }

      inline std::string firstNonEmptyLineIn( const Pathname & file_r )
      {
        std::ifstream idfile( file_r.c_str() );
        for( iostr::EachLine in( idfile ); in; in.next() )
        {
          std::string line( str::trim( *in ) );
          if ( ! line.empty() )
            return line;
        }
        return std::string();
      }
    }

    std::string TargetImpl::targetDistribution() const
    { return baseproductdata( _root ).registerTarget(); }
    // static version:
    std::string TargetImpl::targetDistribution( const Pathname & root_r )
    { return baseproductdata( staticGuessRoot(root_r) ).registerTarget(); }

    std::string TargetImpl::targetDistributionRelease() const
    { return baseproductdata( _root ).registerRelease(); }
    // static version:
    std::string TargetImpl::targetDistributionRelease( const Pathname & root_r )
    { return baseproductdata( staticGuessRoot(root_r) ).registerRelease();}

    Target::DistributionLabel TargetImpl::distributionLabel() const
    {
      Target::DistributionLabel ret;
      parser::ProductFileData pdata( baseproductdata( _root ) );
      ret.shortName = pdata.shortName();
      ret.summary = pdata.summary();
      return ret;
    }
    // static version:
    Target::DistributionLabel TargetImpl::distributionLabel( const Pathname & root_r )
    {
      Target::DistributionLabel ret;
      parser::ProductFileData pdata( baseproductdata( staticGuessRoot(root_r) ) );
      ret.shortName = pdata.shortName();
      ret.summary = pdata.summary();
      return ret;
    }

    std::string TargetImpl::distributionVersion() const
    {
      if ( _distributionVersion.empty() )
      {
        // By default ZYpp looks for /etc/product.d/baseproduct..
        _distributionVersion = baseproductdata( _root ).edition().version();

        if ( _distributionVersion.empty() )
        {
          // ...But the baseproduct method is not expected to work on RedHat derivatives.
          // On RHEL, Fedora and others the "product version" is determined by the first package
          // providing 'redhat-release'. This value is not hardcoded in YUM and can be configured
          // with the $distroverpkg variable.
          rpm::librpmDb::db_const_iterator it;
          if ( it.findByProvides( ZConfig::instance().distroverpkg() ) )
            _distributionVersion = it->tag_version();
        }

        if ( !_distributionVersion.empty() )
          MIL << "Remember distributionVersion = '" << _distributionVersion << "'" << endl;
      }
      return _distributionVersion;
    }
    // static version: (no fallback to init and read rpm db here)
    std::string TargetImpl::distributionVersion( const Pathname & root_r )
    { return baseproductdata( staticGuessRoot(root_r) ).edition().version(); }


    std::string TargetImpl::distributionFlavor() const
    {
      return firstNonEmptyLineIn( home() / "LastDistributionFlavor" );
    }
    // static version:
    std::string TargetImpl::distributionFlavor( const Pathname & root_r )
    {
      return firstNonEmptyLineIn( staticGuessRoot(root_r) / "/var/lib/zypp/LastDistributionFlavor" );
    }

    ///////////////////////////////////////////////////////////////////

    std::string TargetImpl::anonymousUniqueId() const
    {
      return firstNonEmptyLineIn( home() / "AnonymousUniqueId" );
    }
    // static version:
    std::string TargetImpl::anonymousUniqueId( const Pathname & root_r )
    {
      return firstNonEmptyLineIn( staticGuessRoot(root_r) / "/var/lib/zypp/AnonymousUniqueId" );
    }

    ///////////////////////////////////////////////////////////////////

    void TargetImpl::installSrcPackage( const SrcPackage_constPtr & srcPackage_r )
    {
      // provide on local disk
      repo::RepoMediaAccess access_r;
      repo::SrcPackageProvider prov( access_r );
      ManagedFile localfile = prov.provideSrcPackage( srcPackage_r );
      // install it
      rpm().installPackage ( localfile );
    }

    /////////////////////////////////////////////////////////////////
  } // namespace target
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
