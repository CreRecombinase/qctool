
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include "genfile/MissingValue.hpp"

namespace genfile {
	std::ostream& operator<<( std::ostream& o, MissingValue const& v ) {
		return o << "NA" ;
	}
	
	bool MissingValue::operator==( MissingValue const& other ) const {
		return true ;
	}

	bool MissingValue::operator<( MissingValue const& other ) const {
		return false ;
	}
}
