#ifndef SNPDATAPROVIDER_HPP
#define SNPDATAPROVIDER_HPP

#include <iostream>
#include <string>
#include <vector>
#include "../config.hpp"
#if HAVE_BOOST_FUNCTION
#include <boost/function.hpp>
#endif
#include "snp_data_utils.hpp"
#include "wildcard.hpp"
#include "GenomePosition.hpp"

namespace genfile {
	struct SNPDataSourceError: public SNPDataError { char const* what() const throw() { return "SNPDataSourceError" ; } } ;
	struct FileIsInvalidError: public SNPDataSourceError { char const* what() const throw() { return "FileIsInvalidError" ; } } ;
	struct FileHasTrailingDataAfterLastSNP: public FileIsInvalidError { char const* what() const throw() { return "FileHasTrailingDataAfterLastSNP" ; } } ;
	struct FileContainsSNPsOfDifferentSizes: public FileIsInvalidError { char const* what() const throw() { return "FileContainsSNPsOfDifferentSizes" ; } } ;

	// Base class for classes which provide SNP assay data, one snp at a time.
	// After the class is constructed, the intention is that
	// 1. the number_of_samples() and total_number_of_snps() functions return information
	// reflecting the data in the file or source, and
	// 2. The stream accessible through stream() is readable and pointing to the first snp in the file.
	class SNPDataSource: public SNPDataBase
	{
	public:
		typedef std::auto_ptr< SNPDataSource > UniquePtr ;
		// The following methods are factory functions
		static UniquePtr create( std::string const& filename, Chromosome = UnidentifiedChromosome ) ;
		static UniquePtr create( std::string const& filename, Chromosome, CompressionType compression_type ) ;
		static UniquePtr create_chain( std::vector< wildcard::FilenameMatch > const& matches ) ;
		
	public:
		SNPDataSource() ;
		virtual ~SNPDataSource() ;

		void reset_to_start() ;

		#if HAVE_BOOST_FUNCTION
			typedef boost::function< void() > source_reset_callback_t ;
		#else
			typedef void( *source_reset_callback_t )() ;
		#endif

		void set_source_reset_callback( source_reset_callback_t ) ;

		// The next five functions form the main interface for reading snp data. 
		// These typedefs reflect the signatures which the various setter objects
		// needed by these functions must support.
		typedef boost::function< void ( uint32_t ) > IntegerSetter ;
		typedef boost::function< void ( std::string const& ) > StringSetter ;
		typedef boost::function< void ( char ) > AlleleSetter ;
		typedef boost::function< void ( uint32_t ) > SNPPositionSetter ;
		typedef boost::function< void ( Chromosome ) > ChromosomeSetter ;
		typedef boost::function< void ( std::size_t, double, double, double ) > GenotypeProbabilitySetter ;
		
		// Function: read_snp()
		// Read the data for the next snp from the source (and remove it from the source)
		// Store the data using the given setter objects / function pointers.
		// The returned object evaluates to true if the read was successful, otherwise false.
		SNPDataSource& read_snp(
			IntegerSetter set_number_of_samples,
			StringSetter set_SNPID,
			StringSetter set_RSID,
			ChromosomeSetter set_chromosome,
			SNPPositionSetter set_SNP_position,
			AlleleSetter set_allele1,
			AlleleSetter set_allele2,
			GenotypeProbabilitySetter set_genotype_probabilities
		) ;

		// Function: read_next_snp_with_position_in_range()
		// Move forwards through the source until either
		// 1. a snp with a chromosome/position in the given range is found;
		// 2. a snp with chromosome/position greater than the upper bound is found; or
		// 3. the source is exhausted
		// In case 1., read the snp data and return true; otherwise return false.
		bool get_next_snp_with_position_in_range(
			IntegerSetter const& set_number_of_samples,
			StringSetter const& set_SNPID,
			StringSetter const& set_RSID,
			ChromosomeSetter const& set_chromosome,
			SNPPositionSetter const& set_SNP_position,
			AlleleSetter const& set_allele1,
			AlleleSetter const& set_allele2,
			Chromosome chromosome_lower_bound,
			Chromosome chromosome_upper_bound,
			uint32_t position_lower_bound,
			uint32_t position_upper_bound
		) ;

		// Function: read_next_snp_with_specified_position()
		// Move forwards through the source until either
		// 1. a snp with the given position is found;
		// 2. a snp with position greater than the given position is found; or
		// 3. the source is exhausted
		// In case 1., read the snp data and return true; otherwise return false.
		bool get_next_snp_with_specified_position(
			IntegerSetter const& set_number_of_samples,
			StringSetter const& set_SNPID,
			StringSetter const& set_RSID,
			ChromosomeSetter const& set_chromosome,
			SNPPositionSetter const& set_SNP_position,
			AlleleSetter const& set_allele1,
			AlleleSetter const& set_allele2,
			Chromosome specified_chromosome,
			uint32_t specified_SNP_position
		) ;
		
		// As above but take a GenomePosition.
		bool get_next_snp_with_specified_position(
			IntegerSetter const& set_number_of_samples,
			StringSetter const& set_SNPID,
			StringSetter const& set_RSID,
			ChromosomeSetter const& set_chromosome,
			SNPPositionSetter const& set_SNP_position,
			AlleleSetter const& set_allele1,
			AlleleSetter const& set_allele2,
			GenomePosition specified_position
		) ;
		
		// Function: get_snp_identifying_data()
		// Get the SNP ID, RS ID, position, and alleles of the next snp in the source.
		// Repeated calls to this function return the data for the same snp, until a call to
		// read_snp_probability_data() or ignore_snp_probability_data() is made.
		SNPDataSource& get_snp_identifying_data(
			IntegerSetter const& set_number_of_samples,
			StringSetter const& set_SNPID,
			StringSetter const& set_RSID,
			ChromosomeSetter const& set_chromosome,
			SNPPositionSetter const& set_SNP_position,
			AlleleSetter const& set_allele1,
			AlleleSetter const& set_allele2
		) ;

		// Function: read_snp_probability_data()
		// Read the probability data for the next snp in the source, storing it
		// using the given setter object / function pointer.
		// For each snp, you must call get_snp_identifying_data() at least once before
		// calling this function.
		SNPDataSource& read_snp_probability_data(
			GenotypeProbabilitySetter const& set_genotype_probabilities
		) ;

		// Function: ignore_snp_probability_data()
		// Read and discard the probability data for the next snp in the source.
		// For each snp, you must call get_snp_identifying_data() at least once before
		// calling this function.
		SNPDataSource& ignore_snp_probability_data() ;

	public:
		// Return the number of snps which have been read from the source so far.
		std::size_t number_of_snps_read() const { return m_number_of_snps_read ; }

		// Implicit conversion to bool.  Return true if there have been no errors so far.
		virtual operator bool() const = 0 ;
		// Return the number of samples represented in the snps in this source.
		virtual unsigned int number_of_samples() const = 0;
		// Return the total number of snps the source contains.
		virtual unsigned int total_number_of_snps() const = 0 ;

	protected:

		virtual void get_snp_identifying_data_impl( 
			IntegerSetter const& set_number_of_samples,
			StringSetter const& set_SNPID,
			StringSetter const& set_RSID,
			ChromosomeSetter const& set_chromosome,
			SNPPositionSetter const& set_SNP_position,
			AlleleSetter const& set_allele1,
			AlleleSetter const& set_allele2
		) = 0 ;	

		virtual void read_snp_probability_data_impl(
			GenotypeProbabilitySetter const& set_genotype_probabilities
		) = 0 ;

		virtual void ignore_snp_probability_data_impl() = 0 ;
		virtual void reset_to_start_impl() = 0 ;

	protected:

		enum State { e_HaveReadIdentifyingData, e_HaveNotReadIdentifyingData } ;
		State const& state() const { return m_state ; }

	private:
		std::size_t m_number_of_snps_read ;
		SNPDataSource( SNPDataSource const& other ) ;
		SNPDataSource& operator=( SNPDataSource const& other ) ;

		// state variable SNP identifying data
		State m_state ;

		// callbacks
		source_reset_callback_t m_source_reset_callback ;
	} ;

	class IdentifyingDataCachingSNPDataSource: public SNPDataSource
	{
		virtual void read_snp_identifying_data_impl( 
			uint32_t* number_of_samples,
			std::string* SNPID,
			std::string* RSID,
			Chromosome* chromosome,
			uint32_t* SNP_position,
			char* allele1,
			char* allele2
		) = 0 ;
		

		void get_snp_identifying_data_impl( 
			IntegerSetter const& set_number_of_samples,
			StringSetter const& set_SNPID,
			StringSetter const& set_RSID,
			ChromosomeSetter const& set_chromosome,
			SNPPositionSetter const& set_SNP_position,
			AlleleSetter const& set_allele1,
			AlleleSetter const& set_allele2
		) ;

	private:
		bool m_have_cached_identifying_data ;
		Chromosome m_cached_chromosome ;
		uint32_t m_cached_number_of_samples, m_cached_SNP_position ;
		std::string m_cached_SNPID, m_cached_RSID ;
		char m_cached_allele1, m_cached_allele2 ;
	} ;
}

#endif