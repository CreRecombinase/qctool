#include <vector>
#include <iostream>
#include <map>
#include "SampleRow.hpp"
#include "SampleRowStatistics.hpp"
#include "GenotypeProportions.hpp"
#include "GToolException.hpp"
#include "GenotypeAssayStatistics.hpp"

void SampleRowStatistics::process( SampleRow const& row, GenotypeProportions const& amounts, std::size_t number_of_samples ) {
	m_row = &row ;
	base_t::process( &amounts, (&amounts) + 1 ) ;
	set_number_of_samples( number_of_samples ) ;
}

void SampleRowStatistics::add_to_sample_row( SampleRow& row ) const {
	
	for( std::vector< std::string >::const_iterator i = begin_statistics(); i != end_statistics(); ++i ) {
		row.add_column( *i, '0' ) ;
		row.set_value( *i, get_statistic_value< double >( *i )) ;
	}
}

double SampleRowSpecificStatistic::calculate_value( GenotypeAssayStatistics const& statistics ) const {
	return calculate_value( get_row_statistics( statistics )) ;
}

std::string SampleRowSpecificStatistic::calculate_string_value( GenotypeAssayStatistics const& statistics ) const {
	return calculate_string_value( get_row_statistics( statistics )) ;
}

std::string SampleRowSpecificStatistic::calculate_string_value( SampleRowStatistics const& row_statistics ) const {
	return base_t::calculate_string_value( row_statistics ) ;
}

SampleRowStatistics const& SampleRowSpecificStatistic::get_row_statistics( GenotypeAssayStatistics const& statistics ) const {
	try {
		return dynamic_cast< SampleRowStatistics const& >( statistics ) ;
	}
	catch (std::bad_cast const& e ) {
		throw GenotypeAssayStatisticException( "Unable to cast statistics to type SampleRowStatistic.  This statistic only applies to sample rows" ) ;
	}
}

double SampleRowID1::calculate_value( SampleRowStatistics const& row_statistics ) const {
	throw GenotypeAssayStatisticException( "SampleRowID1 does not support double values." ) ;
}

std::string SampleRowID1::calculate_string_value( SampleRowStatistics const& row_statistics ) const {
	return row_statistics.row().ID1() ;
}

double SampleRowID2::calculate_value( SampleRowStatistics const& row_statistics ) const {
	throw GenotypeAssayStatisticException( "SampleRowID2 does not support double values." ) ;
}

std::string SampleRowID2::calculate_string_value( SampleRowStatistics const& row_statistics ) const {
	return row_statistics.row().ID2() ;
}

