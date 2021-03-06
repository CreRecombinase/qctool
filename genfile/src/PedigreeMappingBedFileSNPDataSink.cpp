
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <vector>
#include <utility>
#include <fstream>
#include "genfile/SNPDataSink.hpp"
#include "genfile/CohortIndividualSource.hpp"
#include "genfile/Pedigree.hpp"
#include "genfile/PedigreeMappingBedFileSNPDataSink.hpp"
#include "genfile/snp_data_utils.hpp"
#include "genfile/string_utils.hpp"

namespace genfile {
	namespace {
		std::string sex_to_string( Pedigree::Sex sex ) {
			switch( sex ) {
				case Pedigree::eMale: return "1"; break ;
				case Pedigree::eFemale: return "2"; break ;
				case Pedigree::eUnknown: return "NA"; break ;
			}
			return "NA";
		}
	}
	
	PedigreeMappingBedFileSNPDataSink::PedigreeMappingBedFileSNPDataSink(
		Pedigree::UniquePtr pedigree,
		std::string const& output_bed_filename,
		double call_threshhold
	):
		m_pedigree( pedigree ),
		m_call_threshhold( call_threshhold )
	{
		assert( m_pedigree.get() ) ;
		assert( call_threshhold > 0.5 ) ;
		if( output_bed_filename.size() < 4 || output_bed_filename.substr( output_bed_filename.size() - 4, 4 ) != ".bed" ) {
			throw BadArgumentError( "PedigreeMappingBedFileSNPDataSink::PedigreeMappingBedFileSNPDataSink", "output_bed_filename = \"" + output_bed_filename + "\"" ) ;
		}
		m_output_filename_stub = output_bed_filename.substr( 0, output_bed_filename.size() - 4 ) ;
		setup() ;
	}
	
	std::string PedigreeMappingBedFileSNPDataSink::get_spec() const {
		return m_output_filename_stub ;
	}

	
	PedigreeMappingBedFileSNPDataSink& PedigreeMappingBedFileSNPDataSink::set_sample_names( std::size_t number_of_samples, SampleNameGetter getter ) {
		m_sample_ids.clear() ;
		m_sample_ids.reserve( number_of_samples ) ;
		for( std::size_t i = 0; i < number_of_samples; ++i ) {
			m_sample_ids.push_back( getter(i).as< std::string >() ) ;
		}
		m_pedigree_to_sample_mapping = get_pedigree_to_sample_mapping( *m_pedigree, m_sample_ids ) ;
		if( m_pedigree_to_sample_mapping.empty() ) {
			// No matches between sample file and pedigree could be found.
			// All output genotypes would be NA, let's throw an error in this case.
			throw MismatchError( "genfile::PedFileSNPDataSink::set_sample_names()", "Pedigree / sample file", "Individual id (column 2 of pedigree file)", "ID_1 / ID_2 column." ) ;
		}
		return *this ;
	}

	PedigreeMappingBedFileSNPDataSink& PedigreeMappingBedFileSNPDataSink::set_metadata( Metadata const& ) {
		// do nothing.  Metadata not supported.
		return *this ;
	}
	
	void PedigreeMappingBedFileSNPDataSink::setup() {
		m_bed_file = open_binary_file_for_output( m_output_filename_stub + ".bed", "no_compression" ) ;
		m_bim_file = open_text_file_for_output( m_output_filename_stub + ".bim", "no_compression" ) ;
		
		std::vector< char > data( 3, 0 ) ;
		// BED magic number
		data[0] = 0x6C ;
		data[1] = 0x1B ;
		// Mode is SNP-major
		data[2] = 0x1 ;
		m_bed_file->write( &data[0], 3 ) ;
	}
	
	
	PedigreeMappingBedFileSNPDataSink::~PedigreeMappingBedFileSNPDataSink() {
		write_ped_file( m_output_filename_stub + ".fam" ) ;
	}
	
	void PedigreeMappingBedFileSNPDataSink::write_ped_file( std::string const& output_filename ) const {
		std::auto_ptr< std::ostream> out(
			open_text_file_for_output(
				output_filename,
				get_compression_type_indicated_by_filename( output_filename )
			)
		) ;

		for( std::size_t i = 0; i < m_pedigree->get_number_of_individuals(); ++i ) {
			// first five columns are:
			// family, id, father, mother, sex
			std::string const id = m_pedigree->get_id_of( i ) ;
			(*out) << m_pedigree->get_family_of( id )
				<< " " << id
				<< " " << m_pedigree->get_parents_of( id )[0]
				<< " " << m_pedigree->get_parents_of( id )[1]
				<< " " << sex_to_string( m_pedigree->get_sex_of( id ) ) ;

			// Then comes the phenotype column.
			// PLINK needs exactly six columns here otherwise the poor thing gets confused.
			(*out) << " NA\n" ;
		}
		assert( (*out) ) ;
	}
	
	std::map< std::size_t, std::size_t > PedigreeMappingBedFileSNPDataSink::get_pedigree_to_sample_mapping(
		Pedigree const& pedigree,
		std::vector< std::string > const& sample_ids
	) {
		std::map< std::size_t, std::size_t > result ;

		for( std::size_t i = 0; i < pedigree.get_number_of_individuals(); ++i ) {
			std::string const& id = pedigree.get_id_of( i ) ;
			//std::cerr << "Matching up individual " << i << ": " << id << ".\n" ;
			std::vector< std::string >::const_iterator where = std::find( sample_ids.begin(), sample_ids.end(), id ) ;
			if( where != sample_ids.end() ) {
				// check match is unique.
				std::vector< std::string >::const_iterator next = where ;
				std::vector< std::string >::const_iterator const end = sample_ids.end() ;
				++next ;
				assert( std::find( next, end, id ) == end ) ;
				result[ i ] = ( where - sample_ids.begin() )  ;
			}
		}
		return result ;
	}
	
	void PedigreeMappingBedFileSNPDataSink::write_snp_impl(
		uint32_t number_of_samples,
		std::string SNPID,
		std::string RSID,
		Chromosome chromosome,
		uint32_t SNP_position,
		std::string first_allele,
		std::string second_allele,
		GenotypeProbabilityGetter const& get_AA_probability,
		GenotypeProbabilityGetter const& get_AB_probability,
		GenotypeProbabilityGetter const& get_BB_probability,
		Info const& info
	) {
		assert( number_of_samples == m_sample_ids.size() ) ;
		
		/* Write BIM file line. */
		(*m_bim_file ) << chromosome << " " << RSID << " 0 " << SNP_position << " " << first_allele << " " << second_allele << "\n" ;

		/* Write BED file line */
		std::vector< char > data(( m_pedigree->get_number_of_individuals() + 3 ) / 4, 0 ) ;
		for( std::size_t i = 0; i < m_pedigree->get_number_of_individuals(); ++i ) {
			char d ;
			std::map< std::size_t, std::size_t >::const_iterator where = m_pedigree_to_sample_mapping.find( i ) ;
			if( where == m_pedigree_to_sample_mapping.end() ) {
				// no genotypes for this individual, set it to missing.
				d = 0x2 ;
			}
			else {
				double
					AA = get_AA_probability( i ),
					AB = get_AB_probability( i ),
					BB = get_BB_probability( i ) ;
				if( AA > m_call_threshhold ) {
					d = 0x0 ;
				}
				else if( AB > m_call_threshhold ) {
					d = 0x1 ;
				}
				else if( BB > m_call_threshhold ) {
					d = 0x3 ;
				}
				else {
					d = 0x2 ;
				}
			}
			d <<= ( 2 * ( i % 4 )) ;
			std::size_t c_i = i / 4 ;
			data[c_i] |= d ;
		}

		m_bed_file->write( &data[0], data.size() ) ;
	}
}
