
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include "../config.hpp"
#ifdef HAVE_ZLIB
#include <sstream>
#include <zlib.h>
#endif
#include "genfile/zlib.hpp"
namespace genfile {
	void zlib_compress( char const* buffer, char const* const end, std::vector< char >* dest ) {
		assert( dest != 0 ) ;
		#if HAVE_ZLIB
			uLongf const source_size = ( end - buffer ) ;
			dest->resize( compressBound( source_size ) ) ;
			uLongf compressed_size = dest->size() ;
			int result = compress2(
				reinterpret_cast< Bytef* >( const_cast< char* >( &( dest->operator[](0) ) ) ),
				&compressed_size,
				reinterpret_cast< Bytef const* >( buffer ),
				source_size,
				Z_DEFAULT_COMPRESSION
			) ;
			assert( result == Z_OK ) ;
			dest->resize( compressed_size ) ;
		#else
			assert( 0 ) ; // no zlib support.
		#endif
	}
}
