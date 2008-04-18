/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/parser/tagfile/RepoException.h
 *
*/
#ifndef ZYPP_REPO_REPOEXCEPTION_H
#define ZYPP_REPO_REPOEXCEPTION_H

#include <iosfwd>
#include <string>

#include "zypp/base/Exception.h"
#include "zypp/base/UserRequestException.h"
#include "zypp/RepoInfo.h"
///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace repo
  { /////////////////////////////////////////////////////////////////

    /**
     * \short Exception for repository handling.
     */
    class RepoException : public Exception
    {
    public:
      /** Default ctor */
      RepoException();
      /** Ctor */
      RepoException( const std::string & msg_r );

      RepoException( const RepoInfo & info );

      RepoException( const RepoInfo & info, const std::string & msg_r );

      virtual ~RepoException() throw() {}
      
      RepoInfo info()
      { return _info; }

    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;

    private:
      RepoInfo _info;
    };
    ///////////////////////////////////////////////////////////////////

    /**
     * The repository cache is not built yet
     * so you can't create the repostories from
     * the cache.
     */
    class RepoNotCachedException : public RepoException
    {
    public:
      RepoNotCachedException( const RepoInfo& info );
      RepoNotCachedException( const RepoInfo& info, const std::string & msg_r );
    };
    
    /**
     * thrown when it was impossible to
     * determine one url for this repo.
     */
    class RepoNoUrlException : public RepoException
    {
      public:
      RepoNoUrlException()
      {}
      
      RepoNoUrlException( const RepoInfo &info)
        : RepoException(info)
        {}
      
    };
    
    /**
     * thrown when it was impossible to
     * determine an alias for this repo.
     */
    class RepoNoAliasException : public RepoException
    {
    
    };
    
    /**
     * thrown when it was impossible to
     * match a repository
     */
    class RepoNotFoundException : public RepoException
    {
    public:
      RepoNotFoundException( const RepoInfo &info)
        : RepoException(info)
      {}
    };
    
    /**
     * Repository already exists and some unique
     * attribute can't be duplicated.
     */
    class RepoAlreadyExistsException : public RepoException
    {
    public:
      RepoAlreadyExistsException( const RepoInfo &info,
                                  const std::string & msg_r )
        : RepoException(info,msg_r)
      {}
        
      RepoAlreadyExistsException( const RepoInfo &info )
        : RepoException(info)
      {}
      
      std::string alias()
      { return info().alias(); }
    };
    
    /**
     * thrown when it was impossible to
     * determine this repo type.
     */
    class RepoUnknownTypeException : public RepoException
    {
    public:
      RepoUnknownTypeException( const std::string & msg_r )
        : RepoException(msg_r)
      {}

      RepoUnknownTypeException()
      {}

    };

    /**
     * thrown when it was impossible to
     * use the raw metadata for this repo.
     */
    class RepoMetadataException : public RepoException
    {
    public:
      RepoMetadataException( const RepoInfo &info)
        : RepoException(info)
      {}

      RepoMetadataException()
      {}
    };
    
    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_PARSER_TAGFILE_PARSEEXCEPTION_H