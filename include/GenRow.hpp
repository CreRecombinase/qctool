#ifndef __GTOOL_GENROW_HPP__
#define __GTOOL_GENROW_HPP__

#include <vector>
#include <string>
#include <iostream>
#include "GenotypeProportions.hpp"
#include "genfile/SNPDataSource.hpp"
#include "genfile/SNPDataSink.hpp"
#include "genfile/SNPIdentifyingData.hpp"
#include "genfile/SingleSNPGenotypeProbabilities.hpp"

class GenRowIdentifyingData
{
public:
	typedef genfile::Chromosome Chromosome ;
public:
	GenRowIdentifyingData() ;
	GenRowIdentifyingData( GenRowIdentifyingData const& other ) ;
	GenRowIdentifyingData& operator=( GenRowIdentifyingData const& other ) ;
	GenRowIdentifyingData( genfile::SNPIdentifyingData const& id_data ) ;
	
	std::string SNPID() const { return m_SNPID ; } 
	std::string RSID() const { return m_RSID ; }
	Chromosome chromosome() const { return m_chromosome ; }
	int SNP_position() const { return m_SNP_position ; }
	char first_allele() const { return m_1st_allele; }
	char second_allele() const { return m_2nd_allele; }
	
	// Data setters, needed for genfile support.
	void set_SNPID( std::string const& str ) { m_SNPID = str ; }
	void set_RSID( std::string const& str ) { m_RSID = str ; }
	void set_chromosome( Chromosome chromosome ) { m_chromosome = chromosome ; }
	void set_SNP_position( int pos ) { m_SNP_position = pos ; }
	void set_allele1( char a ) { m_1st_allele = a ; }
	void set_allele2( char b ) { m_2nd_allele = b ; }

	bool operator==( GenRowIdentifyingData const& right ) const ;

public:
	
	std::ostream& write_to_text_stream( std::ostream& ) const ;
	std::istream& read_from_text_stream( std::istream& ) ;
	
private:
	// Data fields
	std::string m_SNPID ;
	std::string m_RSID ;
	Chromosome m_chromosome ;
	int m_SNP_position ;
	char m_1st_allele, m_2nd_allele ;
} ;

std::ostream& operator<<( std::ostream&, GenRowIdentifyingData const& ) ;

class GenRow: public GenRowIdentifyingData
{
	public:
		virtual ~GenRow() {} ;
		GenRow() ;
		GenRow( genfile::SNPIdentifyingData const& id_data ) ;
		GenRow( GenRow const& other ) ;
		GenRow& operator=( GenRow const& other ) ;
		
		std::size_t number_of_samples() const ;
		virtual void set_number_of_samples( std::size_t n ) = 0 ;

		typedef GenotypeProportions const* genotype_proportion_const_iterator ;
		typedef GenotypeProportions* genotype_proportion_iterator ;

		virtual genotype_proportion_const_iterator begin_genotype_proportions() const = 0 ;
		virtual genotype_proportion_const_iterator end_genotype_proportions() const = 0 ;
		virtual genotype_proportion_iterator begin_genotype_proportions() = 0 ;
		virtual genotype_proportion_iterator end_genotype_proportions() = 0 ;

		virtual void add_genotype_proportions( GenotypeProportions const& ) = 0 ;

		genfile::SNPDataSource& read_from_source( genfile::SNPDataSource& snp_data_source ) ;
		genfile::SNPDataSink& write_to_sink( genfile::SNPDataSink& snp_data_sink ) const ;

	public:

		bool operator==( GenRow const& right ) const ;
		bool operator!=( GenRow const& right ) const ;

		std::ostream& write_to_text_stream( std::ostream& ) const ;
		std::istream& read_from_text_stream( std::istream& ) ;

	public:

		void set_genotype_probabilities( std::size_t i, double aa, double ab, double bb ) {
			assert( i < number_of_samples() ) ;
			(begin_genotype_proportions() + i)->AA() = aa ;
			(begin_genotype_proportions() + i)->AB() = ab ;
			(begin_genotype_proportions() + i)->BB() = bb ;
		}

		double get_AA_probability( std::size_t i ) const { return (begin_genotype_proportions() + i)->AA() ; }
		double get_AB_probability( std::size_t i ) const { return (begin_genotype_proportions() + i)->AB() ; }
		double get_BB_probability( std::size_t i ) const { return (begin_genotype_proportions() + i)->BB() ; }

		static bool check_if_equal( GenRow const& left, GenRow const& right, double epsilon = 0.0 ) ;

	private:
	    friend std::istream& operator>>( std::istream&, GenRow& ) ;
} ;


std::ostream& operator<<( std::ostream&, GenRow const& ) ;


class InternalStorageGenRow: public GenRow
{
public:
	InternalStorageGenRow() ;
	InternalStorageGenRow( genfile::SNPIdentifyingData const& id_data, genfile::SingleSNPGenotypeProbabilities const& genotypes ) ;
	InternalStorageGenRow( GenRow const& other ) ;
	InternalStorageGenRow& operator=( GenRow const& other ) ;
	
	void set_number_of_samples( std::size_t n ) { m_genotype_proportions.resize( n ) ; }
	
	virtual genotype_proportion_const_iterator begin_genotype_proportions() const { return (&m_genotype_proportions[0]) ; }
	virtual genotype_proportion_const_iterator end_genotype_proportions() const { return (&m_genotype_proportions[0]) + m_genotype_proportions.size() ; }
	virtual genotype_proportion_iterator begin_genotype_proportions() { return (&m_genotype_proportions[0]) ; }
	virtual genotype_proportion_iterator end_genotype_proportions() { return (&m_genotype_proportions[0]) + m_genotype_proportions.size() ; }

	virtual void add_genotype_proportions( GenotypeProportions const& genotype_proportions ) { m_genotype_proportions.push_back( genotype_proportions ) ; }

	void filter_out_samples_with_indices( std::vector< std::size_t > const& indices_to_filter_out ) ;

private:
	std::vector< GenotypeProportions > m_genotype_proportions ;
} ;




#endif

