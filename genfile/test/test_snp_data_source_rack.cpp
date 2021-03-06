
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <fstream>
#include "test_case.hpp"

#if HAVE_BOOST_FILESYSTEM
	#include <boost/filesystem/operations.hpp>
#endif

#include "genfile/SNPDataSource.hpp"
#include "genfile/SNPDataSourceRack.hpp"
#include "genfile/GenFileSNPDataSource.hpp"
#include "stdint.h"

AUTO_TEST_SUITE( test_snp_data_source_rack )
	
// The following section contains a simple snp block writer.
namespace data {
	// this data has 1504 samples per row.
	unsigned int const number_of_samples = 1504 ;
}

// The following section defines the needed objects for use with the bgen.hpp implementation.
template< typename T >
struct Setter
{
	Setter( T& field ): m_field( field ) {} ;
	template< typename T2 >
	void operator()( T2 const& value ) { m_field = T( value ) ; }
private:
	T& m_field ;
} ;

template< typename T >
Setter< T > make_setter( T& field ) { return Setter<T>( field ) ; }


struct probabilities_t {
	double AA, AB, BB ;
	bool operator==( probabilities_t const& other ) const {
		return AA == other.AA && AB == other.AB && BB == other.BB ;
	}
} ;

struct ProbabilitySetter {
	ProbabilitySetter( std::vector< probabilities_t >& probabilities ): m_probabilities( probabilities ) {}
	void operator() ( std::size_t i, double aa, double ab, double bb ) {
		m_probabilities[i].AA = aa ;  
		m_probabilities[i].AB = ab ;  
		m_probabilities[i].BB = bb ;  
	}

private:
	std::vector<probabilities_t>& m_probabilities ;
} ;

struct ProbabilityGetter {
	ProbabilityGetter( std::vector< probabilities_t >& probabilities, int index ): m_probabilities( probabilities ), m_index( index ) {}
	double operator() ( std::size_t i ) {
		switch( m_index ) {
			case 0: return m_probabilities[i].AA ; break ;
			case 1: return m_probabilities[i].AB ; break ;
			case 2: return m_probabilities[i].BB ; break ;
			default:
			assert(0); 
			break ;
		}
		return 0.0 ;
	}
private:
	std::vector<probabilities_t>& m_probabilities ;
	int m_index ;
} ;

struct SnpData {
	
	SnpData(): probabilities( data::number_of_samples ) {} ;
	genfile::VariantIdentifyingData snp ;
	std::vector< probabilities_t > probabilities ;
	
	bool operator==( SnpData const& other ) const {
		return snp == other.snp && probabilities == other.probabilities ;
	}
} ;


void create_file( std::string const& data, std::string const& filename ) {
	// set up our original file.
	std::ofstream file( filename.c_str() ) ;
	file << data ;
}

std::vector< SnpData > read_snp_data( genfile::SNPDataSource& snp_data_source ) {
	std::vector< SnpData > result ;
	SnpData snp_data ;
	
	while( snp_data_source.get_snp_identifying_data( &snp_data.snp )) {
		snp_data_source.read_snp_probability_data( ProbabilitySetter( snp_data.probabilities )) ;
		result.push_back( snp_data ) ;
	}
	return result ;
}

std::vector< std::vector< std::string > > construct_data() {
	std::vector< std::vector< std::string > > data ;
	data.push_back( std::vector< std::string >() ) ;

	data.push_back( std::vector< std::string >() ) ;
	data.back().push_back( "SNP1 RS1 1000 a g 1 0 0 1 0 0\n" ) ;
	data.back().push_back( "SNP1 RS1 1000 a g 0 0 1 0 0 1\n" ) ;

	data.push_back( std::vector< std::string >() ) ;
	data.back().push_back( "SNP1 RS1 1000 a g 1 0 0 1 0 0\n" ) ;
	data.back().push_back( "SNP1 RS1 1000 a g 0 0 1 0 0 1\n" ) ;
	data.back().push_back( "SNP1 RS1 1000 a g 0 0 1 0 0 1\n" ) ;
	data.back().push_back( "SNP1 RS1 1000 a g 0 0 1 0 0 1\n" ) ;
	data.back().push_back( "SNP1 RS1 1000 a g 0 0 1 0 0 1\n" ) ;

	data.push_back( std::vector< std::string >() ) ;
	data.back().push_back(
		"SNP2 RS2 2000 a g 1 0 0 1 0 0\n"
	) ;
	data.back().push_back(
		"SNP1 RS1 1000 a g 0 0 1 0 0 1\n"
		"SNP2 RS2 2000 a g 0 0 1 0 0 1\n"
	) ;

	data.push_back( std::vector< std::string >() ) ;
	data.back().push_back(
		"SNP2 RS2 2000 a g 1 0 0 1 0 0\n"
	) ;
	data.back().push_back(
		"SNP1 RS1 1000 a g 0 0 1 0 0 1\n"
		"SNP2 RS2 2000 a g 0 0 1 0 0 1\n"
	) ;
	data.back().push_back(
		"SNP2 RS2 2000 a g 0 0 1 0 0 1\n"
	) ;

	data.push_back( std::vector< std::string >() ) ;
	data.back().push_back(
		"SNP1 RS1 1000 a g 0 0 1 0 0 1\n"
		"SNP2 RS2 2000 a g 1 0 0 1 0 0\n"
		"SNP5 RS5 5000 a g 1 0 0 1 0 0\n"
	) ;
	data.back().push_back(
		"SNP1 RS1 1000 a g 0 0 1 0 0 1\n"
		"SNP2 RS2 2000 a g 0 0 1 0 0 1\n"
		"SNP3 RS3 3000 a g 0 0 1 0 0 1\n"
		"SNP4 RS4 4000 a g 0 0 1 0 0 1\n"
		"SNP5 RS5 5000 a g 1 0 0 1 0 0\n"
	) ;
	return data ;
}

AUTO_TEST_CASE( test_snp_data_source_rack ) {
	std::vector< std::vector< std::string > > data = construct_data() ;
	genfile::VariantIdentifyingData snp ;
	for( std::size_t i = 0; i < data.size(); ++i ) {
		std::cout << "==== Looking at test data " << i << " ====\n" ;
		std::auto_ptr< genfile::SNPDataSourceRack > rack( new genfile::SNPDataSourceRack() ) ;
		std::vector< std::string > filenames( data[i].size() ) ;
		for( std::size_t j = 0; j < filenames.size(); ++j ) {
			filenames[j] = tmpnam(0) + std::string( ".gen" ) ;
			create_file( data[i][j], filenames[j] ) ;
			std::auto_ptr< genfile::SNPDataSource > source( genfile::SNPDataSource::create( filenames[j] )) ;
			rack->add_source( source ) ;
		}

		std::size_t number_of_snps = 0 ;
		if( data[i].size() > 0 ) {
			number_of_snps = std::count( data[i].front().begin(), data[i].front().end(), '\n' ) ;
		}
		std::cout << "rack: " << rack->total_number_of_snps() << " SNPs, expected: " << number_of_snps << ".\n" ;
		// We insist that a rack with a single source (or no sources) knows how many SNPs it contains.
		// We do not mandate behaviour when there are more than one source.
		TEST_ASSERT(
			( rack->number_of_sources() <= 1 && rack->total_number_of_snps() && rack->total_number_of_snps().get() == number_of_snps )
				||
			( rack->number_of_sources() > 1 )
		) ;
		if( number_of_snps > 0 ) {
			std::cout << "rack: " << rack->number_of_samples() << " samples, expected: " << 2 * data[i].size() << ".\n" ;
			TEST_ASSERT( rack->number_of_samples() == 2 * data[i].size() ) ;
		}

		std::size_t count = 0 ;
		while( rack->get_snp_identifying_data( &snp )) {
			rack->ignore_snp_probability_data() ;
			++count ;
		}

		TEST_ASSERT( count == number_of_snps ) ;
		
		for( std::size_t j = 0; j < filenames.size(); ++j ) {
			boost::filesystem::remove( filenames[j] ) ;
		}
		
	}
	std::cout << "==== success ====\n" ;
	
}

AUTO_TEST_SUITE_END()

