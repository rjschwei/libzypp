/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/Capability.cc
 *
*/
#include <iostream>

#include "zypp/Capability.h"
#include "zypp/capability/CapabilityImpl.h"
#include "zypp/capability/NullCap.h"

using namespace std;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Capability::Capability
  //	METHOD TYPE : Ctor
  //
  Capability::Capability()
  : _pimpl( capability::NullCap::instance() )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Capability::Capability
  //	METHOD TYPE : Ctor
  //
  Capability::Capability( Impl_Ptr impl_r )
  : _pimpl( impl_r )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Capability::~Capability
  //	METHOD TYPE : Dtor
  //
  Capability::~Capability()
  {}

  const Resolvable::Kind & Capability::refers() const
  { return _pimpl->refers(); }

  std::string Capability::asString() const
  { return _pimpl->asString(); }


  std::string Capability::name() const
  { return "CapabilityName"; }//return _pimpl->name(); }
    
  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const Capability & obj )
  {
    return str << obj._pimpl;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
