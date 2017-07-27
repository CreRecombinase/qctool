
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread/thread.hpp>
#include "genfile/VariantEntry.hpp"
#include "genfile/Error.hpp"
#include "genfile/string_utils.hpp"
#include "db/Connection.hpp"
#include "db/SQLStatement.hpp"
#include "components/SampleSummaryComponent/DBOutputter.hpp"
#include "qcdb/DBOutputter.hpp"

namespace sample_stats {
	DBOutputter::UniquePtr DBOutputter::create( std::string const& filename, std::string const& analysis_name, std::string const& analysis_description, Metadata const& metadata, genfile::CohortIndividualSource const& samples, std::string const& table_name ) {
		return UniquePtr( new DBOutputter( filename, analysis_name, analysis_description, metadata, samples, table_name ) ) ;
	}
	DBOutputter::SharedPtr DBOutputter::create_shared( std::string const& filename, std::string const& analysis_name, std::string const& analysis_description, Metadata const& metadata, genfile::CohortIndividualSource const& samples, std::string const& table_name ) {
		return SharedPtr( new DBOutputter( filename, analysis_name, analysis_description, metadata, samples, table_name ) ) ;
	}

	DBOutputter::DBOutputter( std::string const& filename, std::string const& analysis_name, std::string const& analysis_description, Metadata const& metadata, genfile::CohortIndividualSource const& samples, std::string const& table_name ):
		qcdb::DBOutputter( filename, analysis_name, analysis_description, metadata ),
		m_table_name( table_name ),
		m_samples( samples ),
		m_max_transaction_count( 10000 )
	{
		db::Connection::ScopedTransactionPtr transaction = connection().open_transaction( 240 ) ;
		connection().run_statement(
			"CREATE TABLE IF NOT EXISTS Sample ( "
				"id INTEGER PRIMARY KEY, "
				"identifier TEXT, "
				"UNIQUE( identifier )"
			")"
		) ;

		connection().run_statement(
			"CREATE TABLE IF NOT EXISTS " + m_table_name + " ( "
				"analysis_id INTEGER NOT NULL, "
				"sample_id INTEGER NOT NULL, "
				"variable_id INTEGER NOT NULL, "
				"value TEXT, "
				"FOREIGN KEY( analysis_id ) REFERENCES Entity( id ), "
				"FOREIGN KEY( sample_id ) REFERENCES Sample( id ), "
				"FOREIGN KEY( variable_id ) REFERENCES Entity( id ), "
				"UNIQUE( analysis_id, sample_id, variable_id )"
			")"
		) ;

		connection().run_statement(
			"CREATE VIEW IF NOT EXISTS " + m_table_name + "View AS "
			"  SELECT SD.analysis_id, Analysis.name AS analysis, "
			"    S.id AS sample_id, S.identifier AS sample, "
			"    V.id AS variable_id, V.name AS variable, "
			"    SD.value AS value "
			"  FROM " + m_table_name + " SD "
			"  INNER JOIN Sample S "
			"    ON SD.sample_id = S.id "
			"  INNER JOIN Analysis "
			"    ON Analysis.id = SD.analysis_id"
			"  INNER JOIN Entity V "
			"    ON V.id = SD.variable_id"
		) ;

		construct_statements() ;
		
		m_variable_id = get_or_create_entity( "per-sample variable", "per-sample variable values" ) ;
		
		store_samples( samples ) ;
	}

	void DBOutputter::construct_statements() {
		m_find_sample_statement = connection().get_statement( "SELECT id FROM Sample WHERE identifier == ?1" ) ;
		m_insert_sample_statement = connection().get_statement( "INSERT INTO Sample (identifier) VALUES( ?1 )" ) ;
		m_find_sampledata_statement = connection().get_statement( "SELECT value FROM " + m_table_name + " WHERE analysis_id = ? AND sample_id = ? AND variable_id = ?" ) ;
		m_insert_sampledata_statement = connection().get_statement(
			"INSERT INTO " + m_table_name + " ( analysis_id, sample_id, variable_id, value ) "
			"VALUES( ?1, ?2, ?3, ?4 )"
		) ;
	}

	db::Connection::RowId DBOutputter::analysis_id() const { return qcdb::DBOutputter::analysis_id() ; }

	DBOutputter::~DBOutputter() {
		if( !m_data.empty() ) {
			write_data( m_data ) ;
			m_data.clear() ;
		}
		finalise() ;
	}
	
	void DBOutputter::finalise( long options ) {
	}
	

	void DBOutputter::store_samples( genfile::CohortIndividualSource const& samples ) {
		
		for( std::size_t i = 0; i < samples.get_number_of_individuals(); ++i ) {
			store_sample( samples, i ) ;
		}
	}

	void DBOutputter::store_sample( genfile::CohortIndividualSource const& samples, std::size_t sample ) {
		db::Connection::RowId sample_id = get_or_create_sample( samples.get_entry( sample, "ID_1" ) ) ;
		genfile::CohortIndividualSource::ColumnSpec spec = samples.get_column_spec() ;

		store_sample_data(
			sample_id,
			get_or_create_entity( "index", "index in genotyping file", m_variable_id ),
			genfile::VariantEntry::Integer( sample )
		) ;
	}
	
	void DBOutputter::store_sample_data( db::Connection::RowId const sample_id, db::Connection::RowId const variable_id, genfile::VariantEntry const value ) {
		// Look for identical sample data already there; if we don't find it, insert it.
		m_find_sampledata_statement
			->bind( 1, analysis_id() )
			.bind( 2, sample_id )
			.bind( 3, variable_id )
			.step() ;
		
		if( m_find_sampledata_statement->empty() ) {
			m_insert_sampledata_statement
				->bind( 1, analysis_id() )
				.bind( 2, sample_id )
				.bind( 3, variable_id )
				.bind( 4, value )
				.step() ;

			m_insert_sampledata_statement->reset() ;
		}
		m_find_sampledata_statement->reset() ;
	}
	

	db::Connection::RowId DBOutputter::get_or_create_sample( genfile::VariantEntry const& identifier ) const {
		db::Connection::RowId result ;

		if( identifier.is_missing() ) {
			throw genfile::BadArgumentError( "impl::DBOutputter::get_or_create_sample()", "identifier=NA" ) ;
		}

		m_find_sample_statement
			->bind( 1, identifier )
			.step() ;

		if( m_find_sample_statement->empty() ) {
			m_insert_sample_statement
				->bind( 1, identifier  )
				.step() ;
				
			result = connection().get_last_insert_row_id() ;

			m_insert_sample_statement->reset() ;
		}
		else {
			result = m_find_sample_statement->get< db::Connection::RowId >( 0 ) ;
		}

		m_find_sample_statement->reset() ;

		return result ;
	}
	
	void DBOutputter::store_per_sample_data(
		std::string const& computation_name,
		std::size_t sample,
		std::string const& variable,
		std::string const& description,
		genfile::VariantEntry const& value
	) {
		m_data.resize( m_data.size() + 1 ) ;
		m_data.back().get<0>() = computation_name ;
		m_data.back().get<1>() = sample ;
		m_data.back().get<2>() = variable ;
		m_data.back().get<3>() = description ;
		m_data.back().get<4>() = value ;

		if( m_data.size() == m_max_transaction_count ) {
			write_data( m_data ) ;
			m_data.clear() ;
		}
	}
	
	void DBOutputter::write_data( Data const& data ) {
		db::Connection::ScopedTransactionPtr transaction = connection().open_transaction( 240 ) ; // wait 4 minutes if we have to.

		if( !transaction.get() ) {
			throw genfile::OperationFailedError( "SampleSummaryComponent::DBOutputter::write_data()", connection().get_spec(), "Opening transaction." ) ;
		}
		for( std::size_t i = 0; i < data.size(); ++i ) {
			write_data( data[i].get<1>(), data[i].get<2>(), data[i].get<3>(), data[i].get<4>() ) ;
		}
	}
	
	void DBOutputter::write_data(
		std::size_t sample,
		std::string const& variable,
		std::string const& description,
		genfile::VariantEntry const& value
	) {
		db::Connection::RowId const sample_id = get_or_create_sample( m_samples.get_entry( sample, "ID_1" ) ) ;
		db::Connection::RowId const variable_id = get_or_create_entity( variable, description, m_variable_id ) ;
		m_insert_sampledata_statement
			->bind( 1, analysis_id() )
			.bind( 2, sample_id )
			.bind( 3, variable_id )
			.bind( 4, value )
			.step() ;
		m_insert_sampledata_statement->reset() ;
	}
	
}
