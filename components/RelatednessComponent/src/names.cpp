#include <vector>
#include <string>
#include "genfile/CohortIndividualSource.hpp"
#include "components/RelatednessComponent/names.hpp"

namespace pca {
	std::string get_concatenated_sample_ids( genfile::CohortIndividualSource const* samples, std::size_t i ) {
		return samples->get_entry( i, "id_1" ).as< std::string >() + ":" + samples->get_entry( i, "id_2" ).as< std::string >() ;
	}
	
	std::string string_and_number( std::string const& s, std::size_t i ) {
		return s + genfile::string_utils::to_string( i ) ;
	}
	
	std::string get_entry( std::vector< std::string > const& entries, std::size_t i ) {
		return entries.at( i ) ;
	}
	
}

