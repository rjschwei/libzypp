/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/susetags/PackagesParser.h
 *
*/
#ifndef ZYPP_SOURCE_SUSETAGS_PACKAGESPARSER_H
#define ZYPP_SOURCE_SUSETAGS_PACKAGESPARSER_H

#include <iosfwd>
#include <list>

#include "zypp/base/PtrTypes.h"
#include "zypp/Pathname.h"
#include "zypp/Package.h"
#include "zypp/NVRAD.h"
#include "zypp/source/susetags/SuseTagsImpl.h"
#include "zypp/source/susetags/SuseTagsPackageImpl.h"

typedef zypp::shared_ptr<zypp::source::susetags::SuseTagsPackageImpl> PkgImplPtr;
typedef std::map<zypp::NVRAD, PkgImplPtr> PkgContent;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace source
  { /////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    namespace susetags
    { /////////////////////////////////////////////////////////////////

      /** \deprecated Just temporary.
       * \throws ParseException and others.
      */
      PkgContent parsePackages( Source_Ref source_r, SuseTagsImpl::Ptr, const Pathname & file_r );

      /////////////////////////////////////////////////////////////////
    } // namespace susetags
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace source
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SOURCE_SUSETAGS_PACKAGESPARSER_H
