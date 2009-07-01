#ifndef __GTOOL_ObjectSink_HPP__
#define __GTOOL_ObjectSink_HPP__

#include <cassert>

// Base class for classes to which GenRows can be written.
template< typename Object >
struct ObjectSink
{
public:
	
	virtual ~ObjectSink() {};
	
	virtual ObjectSink& write( Object const& ) = 0;
	virtual operator bool() const = 0 ;
} ;

template< typename Object >
ObjectSink<Object> & operator<<( ObjectSink< Object >& sink, Object const& object ) {
	sink.write( object ) ;
	return sink ;
}

template< typename Object >
struct NullObjectSink: public ObjectSink< Object >
{
	NullObjectSink& write( Object const& ) {
		return *this ;
	}
	operator bool() const {
		return true ;
	}
} ;

#endif
