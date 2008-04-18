/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/SolvAttr.cc
 *
*/
extern "C"
{
#include <satsolver/knownid.h>
}

#include <iostream>

#include "zypp/base/String.h"
#include "zypp/sat/SolvAttr.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
namespace sat
{ /////////////////////////////////////////////////////////////////

  const SolvAttr SolvAttr::allAttr( detail::noId );
  const SolvAttr SolvAttr::noAttr;

#warning STILL ATTRIBUTES HERE WHICH ARE NOT PROVIDED BY SOLV FILES
// At least the ones that do nat have a satsolver/knownid.

  const SolvAttr SolvAttr::name         ( SOLVABLE_NAME );
  const SolvAttr SolvAttr::summary      ( SOLVABLE_SUMMARY );       // translated
  const SolvAttr SolvAttr::description  ( SOLVABLE_DESCRIPTION );   // translated
  const SolvAttr SolvAttr::insnotify    ( SOLVABLE_MESSAGEINS );    // translated
  const SolvAttr SolvAttr::delnotify    ( SOLVABLE_MESSAGEDEL );    // translated
  const SolvAttr SolvAttr::eula		( SOLVABLE_EULA );          // translated
  const SolvAttr SolvAttr::installtime  ( SOLVABLE_INSTALLTIME );
  const SolvAttr SolvAttr::buildtime    ( SOLVABLE_BUILDTIME );
  const SolvAttr SolvAttr::installsize  ( SOLVABLE_INSTALLSIZE );
  const SolvAttr SolvAttr::downloadsize ( SOLVABLE_DOWNLOADSIZE );
  const SolvAttr SolvAttr::diskusage    ( SOLVABLE_DISKUSAGE );

  //package
  const SolvAttr SolvAttr::checksum     ( SOLVABLE_CHECKSUM );
  const SolvAttr SolvAttr::medianr	( SOLVABLE_MEDIANR );
  const SolvAttr SolvAttr::mediafile	( SOLVABLE_MEDIAFILE );
  const SolvAttr SolvAttr::mediadir	( SOLVABLE_MEDIADIR );
  const SolvAttr SolvAttr::changelog    ( "changelog" );
  const SolvAttr SolvAttr::buildhost    ( "buildhost" );
  const SolvAttr SolvAttr::distribution ( "distribution" );
  const SolvAttr SolvAttr::license      ( SOLVABLE_LICENSE );
  const SolvAttr SolvAttr::packager     ( "packager" );
  const SolvAttr SolvAttr::group        ( SOLVABLE_GROUP );
  const SolvAttr SolvAttr::keywords     ( SOLVABLE_KEYWORDS );
  const SolvAttr SolvAttr::sourcesize   ( "sourcesize" );
  const SolvAttr SolvAttr::authors      ( SOLVABLE_AUTHORS );
  const SolvAttr SolvAttr::filenames    ( "filenames" );
  const SolvAttr SolvAttr::filelist     ( SOLVABLE_FILELIST );
  const SolvAttr SolvAttr::sourcearch   ( SOLVABLE_SOURCEARCH );
  const SolvAttr SolvAttr::sourcename   ( SOLVABLE_SOURCENAME );
  const SolvAttr SolvAttr::sourceevr    ( SOLVABLE_SOURCEEVR );
  const SolvAttr SolvAttr::headerend    ( SOLVABLE_HEADEREND );

  // patch
  const SolvAttr SolvAttr::patchcategory( SOLVABLE_PATCHCATEGORY );
  const SolvAttr SolvAttr::needReboot   ( UPDATE_REBOOT );
  const SolvAttr SolvAttr::needRestart  ( UPDATE_RESTART );
  const SolvAttr SolvAttr::updateCollectionName     ( UPDATE_COLLECTION_NAME );
  const SolvAttr SolvAttr::updateCollectionEvr      ( UPDATE_COLLECTION_EVR );
  const SolvAttr SolvAttr::updateCollectionArch     ( UPDATE_COLLECTION_ARCH );
  const SolvAttr SolvAttr::updateCollectionFilename ( UPDATE_COLLECTION_FILENAME );
  const SolvAttr SolvAttr::updateCollectionFlags    ( UPDATE_COLLECTION_FLAGS );
  const SolvAttr SolvAttr::updateReferenceType      ( UPDATE_REFERENCE_TYPE );
  const SolvAttr SolvAttr::updateReferenceHref      ( UPDATE_REFERENCE_HREF );
  const SolvAttr SolvAttr::updateReferenceId        ( UPDATE_REFERENCE_ID );
  const SolvAttr SolvAttr::updateReferenceTitle     ( UPDATE_REFERENCE_TITLE );

  //pattern
  const SolvAttr SolvAttr::isvisible    ( SOLVABLE_ISVISIBLE );
  const SolvAttr SolvAttr::icon         ( SOLVABLE_ICON );
  const SolvAttr SolvAttr::order        ( SOLVABLE_ORDER );
  const SolvAttr SolvAttr::isdefault    ( "isdefault" );
  const SolvAttr SolvAttr::category     ( SOLVABLE_CATEGORY );    // translated
  const SolvAttr SolvAttr::script       ( "script" );
  const SolvAttr SolvAttr::includes     ( SOLVABLE_INCLUDES );
  const SolvAttr SolvAttr::extends      ( SOLVABLE_EXTENDS );

} // namespace sat
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////