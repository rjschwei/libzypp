/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/repo/cached/SrcPackageImpl.cc
*/

#include "SrcPackageImpl.h"
#include "zypp/TranslatedText.h"
#include "zypp/base/String.h"
#include "zypp/base/Logger.h"
#include "zypp/cache/CacheAttributes.h"

using namespace std;
using namespace zypp::detail;

///////////////////////////////////////////////////////////////////
namespace zypp { namespace repo { namespace cached {

///////////////////////////////////////////////////////////////////
//
//        CLASS NAME : SrcPackageImpl
//
///////////////////////////////////////////////////////////////////

/** Default ctor
*/
SrcPackageImpl::SrcPackageImpl ( const data::RecordId & id, repo::cached::RepoImpl::Ptr repository_r )
  : _repository( repository_r )
  , _id( id )
{}

Repository SrcPackageImpl::repository() const
{
  return _repository->selfRepository();
}

///////////////////////////////////////////////////
// ResObject Attributes
///////////////////////////////////////////////////

TranslatedText SrcPackageImpl::summary() const
{
  return _repository->resolvableQuery().queryTranslatedStringAttribute( _id, cache::attrResObjectSummary() );
}

TranslatedText SrcPackageImpl::description() const
{
  return _repository->resolvableQuery().queryTranslatedStringAttribute( _id, cache::attrResObjectDescription() );
}

TranslatedText SrcPackageImpl::insnotify() const
{
  return _repository->resolvableQuery().queryTranslatedStringAttribute( _id, cache::attrResObjectInsnotify() );
}

TranslatedText SrcPackageImpl::delnotify() const
{
  return _repository->resolvableQuery().queryTranslatedStringAttribute( _id, cache::attrResObjectDelnotify() );
}

TranslatedText SrcPackageImpl::licenseToConfirm() const
{
  return _repository->resolvableQuery().queryTranslatedStringAttribute( _id, cache::attrResObjectLicenseToConfirm() );
}

Vendor SrcPackageImpl::vendor() const
{
  return _repository->resolvableQuery().queryStringAttribute( _id, cache::attrResObjectVendor() );
}

ByteCount SrcPackageImpl::size() const
{
  return _repository->resolvableQuery().queryNumericAttribute( _id, cache::attrResObjectInstalledSize() );
}

bool SrcPackageImpl::installOnly() const
{
  return _repository->resolvableQuery().queryBooleanAttribute( _id, cache::attrResObjectInstallOnly() );
}

Date SrcPackageImpl::buildtime() const
{
  return _repository->resolvableQuery().queryNumericAttribute( _id, cache::attrResObjectBuildTime() );
}

Date SrcPackageImpl::installtime() const
{
  return Date();
}

////////////////////////////////////////////////////////
// SRC PACKAGE
////////////////////////////////////////////////////////

unsigned SrcPackageImpl::mediaNr() const
{
  if ( _mnr == (unsigned)-1 )
  {
    _mnr = _repository->resolvableQuery().queryNumericAttribute( _id, cache::attrSrcPackageLocationMediaNr() );
  }
  return _mnr;
}

ByteCount SrcPackageImpl::downloadSize() const
{
  return _repository->resolvableQuery().queryNumericAttribute( _id, cache::attrSrcPackageLocationDownloadSize() );
}

const DiskUsage & SrcPackageImpl::diskusage() const
{
  if ( ! _diskusage )
  {
    // lazy init
    _diskusage.reset( new DiskUsage );
    _repository->resolvableQuery().queryDiskUsage( _id, *_diskusage );
    if ( _diskusage->empty() )
    {
      // No info: Creating a faked entry distributing the
      // total source package size in assumed 20 files to "/usr/src/packages"
      // The reported size are 1K blocks.
      unsigned dusize = size().blocks( ByteCount::K );
      _diskusage->add( "/",                  dusize, 20 );
      _diskusage->add( "/usr/",              dusize, 20 );
      _diskusage->add( "/usr/src/",          dusize, 20 );
      _diskusage->add( "/usr/src/packages/", dusize, 20 );
    }
  }
  return *_diskusage;
}

OnMediaLocation SrcPackageImpl::location() const
{
  OnMediaLocation loc;
  queryOnMediaLocation( _repository->resolvableQuery(), _id, cache::attrSrcPackageLocation, loc );
  return loc;
}

/////////////////////////////////////////////////////////////////
} } } // namespace zypp.repo.cached
///////////////////////////////////////////////////////////////////