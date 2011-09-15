#ifndef QCTOOL_VCDB_WRITER_HPP
#define QCTOOL_VCDB_WRITER_HPP

#include <boost/noncopyable.hpp>
#include "genfile/SNPDataSourceProcessor.hpp"
#include "genfile/SNPIdentifyingData.hpp"
#include "db/SQLite3Connection.hpp"
#include "DataStore.hpp"

class VCDBWriter: public genfile::SNPDataSourceProcessor::Callback, public boost::noncopyable {
public:
	typedef std::auto_ptr< VCDBWriter > UniquePtr ;
	static UniquePtr create( std::string const& filename ) ;
public:
	
	VCDBWriter( DataStore::UniquePtr store ) ;
	void begin_processing_snps( std::size_t number_of_samples, std::size_t number_of_snps ) ;
	void processed_snp( genfile::SNPIdentifyingData const& , genfile::VariantDataReader& data_reader ) ;
	void end_processing_snps() ;
	void set_SNPs( std::vector< genfile::SNPIdentifyingData > const& snps ) ;
private:
	DataStore::UniquePtr m_store ;
	std::size_t m_number_of_samples ;
	std::size_t m_number_of_snps ;
	std::size_t m_number_of_snps_written ;

	DataStore::EntityId m_cohort_id ;
	DataStore::EntityId m_storage_id ;

	typedef std::map< std::string, db::Connection::RowId > EntityCache ;
	EntityCache m_entity_cache ;

	std::vector< std::vector< genfile::VariantEntry > > m_data ;
	std::vector< char > m_buffer ;
	std::vector< char > m_compressed_buffer ;
	void setup() ;
	db::Connection::RowId get_or_create_field( std::string const& field, std::string const& type ) ;
} ;

#endif
