#ifndef GENFILE_VCF_FORMAT_SNP_DATA_SOURCE_HPP
#define GENFILE_VCF_FORMAT_SNP_DATA_SOURCE_HPP

#include "genfile/SNPDataSource.hpp"
#include "genfile/VCFFormatMetaDataParser.hpp"

namespace genfile {
	class VCFFormatSNPDataSource: public SNPDataSource
	// SNPDataSource which obtains data from a file in VCF format.
	// I used the 4.1 spec, available here:
	// http://www.1000genomes.org/wiki/Analysis/Variant Call Format/vcf-variant-call-format-version-41
	// to implement this.
	{
	public:
		VCFFormatSNPDataSource( std::auto_ptr< std::istream > stream_ptr ) ;
		VCFFormatSNPDataSource( std::string const& filename ) ;

	public:
		operator bool() const ;
		unsigned int number_of_samples() const ;
		unsigned int total_number_of_snps() const ;
		std::string get_source_spec() const ;
		std::string get_summary( std::string const& prefix = "", std::size_t column_width = 20 ) const ;

	protected:

		void get_snp_identifying_data_impl( 
			IntegerSetter const& set_number_of_samples,
			StringSetter const& set_SNPID,
			StringSetter const& set_RSID,
			ChromosomeSetter const& set_chromosome,
			SNPPositionSetter const& set_SNP_position,
			AlleleSetter const& set_allele1,
			AlleleSetter const& set_allele2
		) ;	

		void read_snp_probability_data_impl(
			GenotypeProbabilitySetter const& set_genotype_probabilities
		) ;

		void ignore_snp_probability_data_impl() ;
		void reset_to_start_impl() ;

	private:
		std::string const m_spec ;
		std::auto_ptr< std::istream > m_stream_ptr ;
		VCFFormatMetaDataParser::Metadata const m_metadata ;
		std::vector< std::string > const m_column_names ;
		std::ios::streampos const m_start_of_data ;
		std::size_t const m_number_of_lines ;

	private:
		std::vector< std::string > read_column_names( std::istream& stream ) const ;
		std::size_t count_lines( std::istream& ) const ;
		void reset_stream() const ;

	private:
		VCFFormatSNPDataSource( VCFFormatSNPDataSource const& other ) ;
	} ;
}

#endif
