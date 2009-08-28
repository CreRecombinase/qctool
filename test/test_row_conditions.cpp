#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <map>
#include <string>
#include "GenRow.hpp"
#include "GenRowStatistics.hpp"
#include "SimpleGenotypeAssayStatistics.hpp"
#include "floating_point_utils.hpp"
#include "RowCondition.hpp"
#include "../config.hpp"
#if HAVE_BOOST_UNIT_TEST
	#define BOOST_AUTO_TEST_MAIN
	#include "boost/test/auto_unit_test.hpp"
	#define AUTO_TEST_CASE( param ) BOOST_AUTO_TEST_CASE(param)
	#define TEST_ASSERT( param ) BOOST_ASSERT( param )
#else
	#define AUTO_TEST_CASE( param ) void param()
	#define TEST_ASSERT( param ) assert( param )
#endif

struct TestStringToValueMap: public string_to_value_map {

	bool has_value( std::string const& name ) const {
		return m_values.find( name ) != m_values.end() ;
	}

	double get_double_value( std::string const& name ) const {
		return m_values.find( name )->second ;
	}

	std::string get_string_value( std::string const& name ) const {
		assert(0) ;
	}

	std::map< std::string, double > m_values ;
} ;

std::vector< TestStringToValueMap > get_data() {
	std::vector< TestStringToValueMap > result ;
	for( int i = 0; i < 1000; ++i ) {
		result.push_back( TestStringToValueMap() ) ;
		result.back().m_values[ "value" ] = static_cast< double >(i) / 1000 ;
	}
	return result ;
}

AUTO_TEST_CASE( test_less_than_greater_than ) {
	std::vector< TestStringToValueMap > data = get_data() ;
	
	std::vector< TestStringToValueMap >::const_iterator
		i( data.begin() ),
		end_i( data.end() ) ;

	StatisticGreaterThan gt_condition( "value", 0.5 ) ;
	StatisticLessThan lt_condition( "value", 0.5 ) ;

	for( ; i != end_i; ++i ) {
		double value = i->get_value< double >( "value" ) ;
		TEST_ASSERT( gt_condition.check_if_satisfied( *i ) == (value > 0.5)) ;
		TEST_ASSERT( lt_condition.check_if_satisfied( *i ) == (value < 0.5)) ;
	}
}

AUTO_TEST_CASE( test_range ) {
	std::vector< TestStringToValueMap > data = get_data() ;
	
	std::vector< TestStringToValueMap >::const_iterator
		i( data.begin() ),
		end_i( data.end() ) ;

	StatisticInInclusiveRange inclusive_condition( "value", 0.3, 0.7 ) ;
	StatisticInExclusiveRange exclusive_condition( "value", 0.3, 0.7 ) ;

	for( ; i != end_i; ++i ) {
		double value = i->get_value< double >( "value" ) ;
		TEST_ASSERT( inclusive_condition.check_if_satisfied( *i ) == ((value >= 0.3) && (value <= 0.7))) ;
		TEST_ASSERT( exclusive_condition.check_if_satisfied( *i ) == ((value > 0.3) && (value < 0.7))) ;
	}
}

#ifndef HAVE_BOOST_UNIT_TEST
	int main( int argc, char** argv ) {
		test_less_than_greater_than() ;
		test_range() ;
	}
#endif