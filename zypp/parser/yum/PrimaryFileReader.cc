/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/yum/PrimaryFileReader.cc
 * Implementation of primary.xml.gz file reader.
 */
#include "zypp/base/Logger.h"
#include "zypp/ZConfig.h"

#include "zypp/parser/xml/Reader.h"
#include "zypp/data/ResolvableData.h"
#include "zypp/base/UserRequestException.h"

#include "zypp/parser/yum/FileReaderBaseImpl.h"
#include "zypp/parser/yum/PrimaryFileReader.h"

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "parser::yum"

using namespace std;
using namespace zypp::xml;

namespace zypp
{
  namespace parser
  {
    namespace yum
    {


  ///////////////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : PrimaryFileReader::Impl
  //
  class PrimaryFileReader::Impl : public BaseImpl
  {
  public:
    /** CTOR */
    Impl(const Pathname & primary_file,
         const ProcessPackage & callback,
         const ProgressData::ReceiverFnc & progress);

    /**
     * Callback provided to the XML reader.
     * 
     * This is the main parsing method which gets envoked by the \ref xml::Reader
     * to process each node, one at a time. It is responsible for parsing the
     * node and calling all other consume* methods.
     *
     * \param  the xml reader object reading the file  
     * \return true to tell the reader to continue, false to tell it to stop
     * \throws AbortRequestException if user request for aborting the parsing
     *         has been received through progress reporting callback.
     *
     * \note Semantics of this method's return value also differs from other
     *       consume* methods. While this method's return value tells the the
     *       xml reader to continue or stop, return value of the rest tells
     *       their callers if further
     */
    bool consumeNode(xml::Reader & reader_r);

  private:
    /**
     * Creates a new \ref data::Package_Ptr, swaps its contents with \ref
     * _package and returns it. Used to hand-out the data object to its consumer
     * (a \ref ProcessPackage function) after it has been read.
     */
    data::Packagebase_Ptr handoutPackage();

  private:
    /**
     * Callback for processing package metadata passed in through constructor.
     */
    ProcessPackage _callback;

    /**
     * \ref zypp::data::Packagebase object for storing the package metada.
     * This can be either a data::Package or data::SrcPackage
     */
    data::Packagebase_Ptr _package;

    /**
     * Progress reporting object.
     */
    ProgressData _ticks;

    /**
     * system architecture, to filter out incompatible packages
     */
    Arch _sysarch;
  };
  ///////////////////////////////////////////////////////////////////////////


  PrimaryFileReader::Impl::Impl(
      const Pathname & primary_file,
      const ProcessPackage & callback,
      const ProgressData::ReceiverFnc & progress)
    :
      _callback(callback)
    , _sysarch( ZConfig::instance().systemArchitecture() )
  {
    _ticks.sendTo(progress);

    Reader reader(primary_file);
    MIL << "Reading " << primary_file << endl;
    reader.foreachNode(bind( &PrimaryFileReader::Impl::consumeNode, this, _1 ));
  }

  // --------------------------------------------------------------------------

  bool PrimaryFileReader::Impl::consumeNode(Reader & reader_r)
  {
//    DBG << "**node: " << reader_r->name() << " (" << reader_r->nodeType() << ")" << endl;
    if (isBeingProcessed(tag_package) && consumePackageNode(reader_r, _package))
      return true;


    if (reader_r->nodeType() == XML_READER_TYPE_ELEMENT)
    {
      // xpath: /metadata
      if (reader_r->name() == "metadata")
      {
        unsigned total_packages;
        zypp::str::strtonum(reader_r->getAttribute("packages").asString(), total_packages);
        _ticks.range(total_packages);
        _ticks.toMin();
        return true;
      }

      // xpath: /metadata/package (+)
      if (reader_r->name() == "package")
      {
        tag(tag_package);
  //      DBG << "got " << reader_r->getAttribute("type") << " package" << endl;
        _package = new data::Package;

        return consumePackageNode(reader_r, _package);
      }
    }

    else if ( reader_r->nodeType() == XML_READER_TYPE_END_ELEMENT )
    {
      // xpath: /metadata/package (+)
      if (reader_r->name() == "package")
      {
        if (_package && _callback
	    && Arch(_package->arch).compatibleWith( _sysarch ))
	{
          _callback(handoutPackage());
	}
        if (!_ticks.incr())
          ZYPP_THROW(AbortRequestException());

        toParentTag();
        return true;
      }

      // xpath: /metdata
      if (reader_r->name() == "metadata")
      {
        _ticks.toMax();
        return true;
      }
    }

    return true;
  }

  // --------------------------------------------------------------------------

  data::Packagebase_Ptr PrimaryFileReader::Impl::handoutPackage()
  {
    data::Packagebase_Ptr ret;
    ret.swap(_package);
    return ret;
  }

  ///////////////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : PrimaryFileReader
  //
  ///////////////////////////////////////////////////////////////////////////

  PrimaryFileReader::PrimaryFileReader(
      const Pathname & primary_file,
      const ProcessPackage & callback,
      const ProgressData::ReceiverFnc & progress)
    : _pimpl(new PrimaryFileReader::Impl(primary_file, callback, progress))
  {}


  PrimaryFileReader::~PrimaryFileReader()
  {}


    } // ns yum
  } // ns parser
} //ns zypp

// vim: set ts=2 sts=2 sw=2 et ai: