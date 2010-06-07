#include <iostream>
#include <string>
#include "genfile/snp_data_utils.hpp"
#include "genfile/gen.hpp"
#include "genfile/SNPDataSource.hpp"
#include "genfile/GenFileSNPDataSource.hpp"

namespace genfile {
	GenFileSNPDataSource::GenFileSNPDataSource( std::string const& filename, Chromosome chromosome )
		: m_filename( filename ),
		  m_number_of_samples( 0 ),
		  m_total_number_of_snps( 0 ),
		  m_chromosome( chromosome )
	{
		setup( filename, get_compression_type_indicated_by_filename( filename )) ;
	}

	GenFileSNPDataSource::GenFileSNPDataSource( std::string const& filename, Chromosome chromosome, CompressionType compression_type )
		: m_filename( filename ),
		  m_compression_type( compression_type ),
		  m_number_of_samples( 0 ),
		  m_total_number_of_snps( 0 ),
		  m_chromosome( chromosome )
	{
		setup( filename, compression_type ) ;
	}

	void GenFileSNPDataSource::setup( std::string const& filename, CompressionType compression_type ) {
		m_stream_ptr = open_text_file_for_input( filename, compression_type ) ;
		read_header_data() ;				
		// That will have consumed the file, so re-open it.
		m_stream_ptr = open_text_file_for_input( filename, compression_type ) ;
	}

	void GenFileSNPDataSource::reset_to_start_impl() {
		stream().clear() ;
		stream().seekg( 0 ) ;
		if( !stream() ) {
			// oh, dear, seeking failed.  Reopen the file instead
			m_stream_ptr = open_text_file_for_input( m_filename, m_compression_type ) ;
		}
		if( !stream() ) {
			throw OperationFailedError() ;
		}
	}
	
	void GenFileSNPDataSource::read_snp_identifying_data_impl( 
		uint32_t* number_of_samples, // number_of_samples is unused.
		std::string* SNPID,
		std::string* RSID,
		Chromosome* chromosome,
		uint32_t* SNP_position,
		char* allele1,
		char* allele2
	) {
		gen::impl::read_snp_identifying_data( stream(), SNPID, RSID, SNP_position, allele1, allele2 ) ;
		*number_of_samples = m_number_of_samples ;
		*chromosome = m_chromosome ;
	}

	void GenFileSNPDataSource::read_snp_probability_data_impl(
		GenotypeProbabilitySetter const& set_genotype_probabilities
	) {
		uint32_t this_data_number_of_samples ;
		gen::impl::read_snp_probability_data( stream(), set_value( this_data_number_of_samples ), set_genotype_probabilities ) ;
		if( *this ) {
			assert( this_data_number_of_samples == number_of_samples() ) ;
		}
	}

	void GenFileSNPDataSource::ignore_snp_probability_data_impl() {
		std::string line ;
		std::getline( stream(), line ) ;
	}

	void GenFileSNPDataSource::read_header_data() {
		gen::read_header_information(
			*m_stream_ptr,
			set_value( m_total_number_of_snps ),
			set_value( m_number_of_samples ),
			ignore()
		) ;
	}
}
