
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __GTOOL_STRING_TO_VALUE_MAP_HPP
#define __GTOOL_STRING_TO_VALUE_MAP_HPP


#include <string>


// Base class for objects which map strings to values.
// The values can be returned either as string or double values.
class string_to_value_map
{
public:
	
	virtual ~string_to_value_map() {} ;
	
	template< typename T >
	T get_value( std::string const& name ) const ;

	virtual bool has_value( std::string const& name ) const = 0 ;

protected:

	virtual double get_double_value( std::string const& name ) const = 0 ;
	virtual std::string get_string_value( std::string const& name ) const = 0 ;
} ;


#endif
