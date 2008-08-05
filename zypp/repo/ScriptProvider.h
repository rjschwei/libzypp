/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/ScriptProvider.h
 *
*/
#ifndef ZYPP_REPO_SCRIPTPROVIDER_H
#define ZYPP_REPO_SCRIPTPROVIDER_H

#include <iosfwd>

#include "zypp/base/NonCopyable.h"
#include "zypp/base/PtrTypes.h"

#include "zypp/repo/RepoProvideFile.h"
#include "zypp/ManagedFile.h"
#include "zypp/ResTraits.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace repo
  { /////////////////////////////////////////////////////////////////

    class RepoMediaAccess;

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : ScriptProvider
    //
    /** */
    class ScriptProvider : private base::NonCopyable
    {
    public:
      /** Ctor */
      ScriptProvider( repo::RepoMediaAccess & access_r );
      /** Dtor */
      ~ScriptProvider();

    public:
      /** Provide a script in a local file.*/
      ManagedFile provideScript( const Script_constPtr & script_r, bool do_r ) const
      { return( do_r ? provideDoScript( script_r ) : provideUndoScript( script_r ) ); }

      /** Provide the do-script in a local file.
       * Returns an empty path if no script is available.
      */
      ManagedFile provideDoScript( const Script_constPtr & script_r ) const;

      /** Provide the do-script in a local file.
       * Returns an empty path if no script is available.
      */
      ManagedFile provideUndoScript( const Script_constPtr & script_r ) const;

    private:
      RepoMediaAccess & _access;
    };
    ///////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////
  } // namespace repo
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_REPO_SCRIPTPROVIDER_H