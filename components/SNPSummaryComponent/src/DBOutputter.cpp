
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread/thread.hpp>
#include <boost/format.hpp>
#include "genfile/VariantIdentifyingData.hpp"
#include "genfile/VariantEntry.hpp"
#include "genfile/Error.hpp"
#include "db/Connection.hpp"
#include "db/SQLStatement.hpp"
#include "qcdb/Storage.hpp"
#include "appcontext/get_current_time_as_string.hpp"
#include "components/SNPSummaryComponent/DBOutputter.hpp"

namespace snp_summary_component {
	DBOutputter::UniquePtr DBOutputter::create( std::string const& filename, std::string const& analysis_name, std::string const& analysis_description, Metadata const& metadata ) {
		return UniquePtr( new DBOutputter( filename, analysis_name, analysis_description, metadata ) ) ;
	}
	DBOutputter::SharedPtr DBOutputter::create_shared( std::string const& filename, std::string const& analysis_name, std::string const& analysis_description, Metadata const& metadata ) {
		return SharedPtr( new DBOutputter( filename, analysis_name, analysis_description, metadata ) ) ;
	}

	DBOutputter::DBOutputter(
		std::string const& filename,
		std::string const& analysis_name,
		std::string const& analysis_description,
		Metadata const& metadata
	):
		m_outputter( filename, analysis_name, analysis_description, metadata ),
		m_max_transaction_count( 10000 ),
		m_variable_id( m_outputter.get_or_create_entity( "per-variant variable", "per-variant variable values" ) )
	{
	}

	DBOutputter::~DBOutputter() {
		// Re-finalise.
		write_data( m_data ) ;
		m_data.clear() ;
		m_outputter.finalise() ;
	}
	
	void DBOutputter::set_table_name( std::string const& table_name ) {
		if( table_name.find_first_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_" ) != std::string::npos ) {
			throw genfile::BadArgumentError( "qcdb::DBOutputter::set_table_name()", "table_name=\"" + table_name + "\"" ) ;
		}
		m_table_name = table_name ;
	}
	
	void DBOutputter::finalise( long options ) {
		write_data( m_data ) ;
		m_data.clear() ;
		m_outputter.finalise( options ) ;
		if( options & qcdb::eCreateIndices ) {
			m_outputter.connection().run_statement(
				"CREATE INDEX IF NOT EXISTS SummaryDataIndex ON SummaryData( variant_id, variable_id )"
			) ;
		}
	}

	db::Connection::RowId DBOutputter::analysis_id() const {
		return m_outputter.analysis_id() ;
	}

	void DBOutputter::add_variable(
		std::string const& 
	) {
		// nothing to do.  We do not report values for unused variables.
	}

	void DBOutputter::create_new_variant( genfile::VariantIdentifyingData const& snp ) {
		// nothing to do.
	}

	void DBOutputter::store_per_variant_data(
		genfile::VariantIdentifyingData const& snp,
		std::string const& variable,
		genfile::VariantEntry const& value
	) {
		m_data.resize( m_data.size() + 1 ) ;
		m_data.back().get<0>() = snp ;
		m_data.back().get<1>() = variable ;
		m_data.back().get<2>() = value ;

		if( m_data.size() == m_max_transaction_count ) {
			write_data( m_data ) ;
			m_data.clear() ;
		}
	}
	void DBOutputter::create_schema() {
		m_outputter.connection().run_statement(
			(
				boost::format(
					"CREATE TABLE IF NOT EXISTS %s ( "
					"variant_id INT, analysis_id INT, variable_id INT, value NONE, "
					"FOREIGN KEY( variant_id ) REFERENCES Variant( id ), "
					"FOREIGN KEY( analysis_id ) REFERENCES Entity( id ), "
					"FOREIGN KEY( variable_id ) REFERENCES Entity( id ) "
					")"
				) % m_table_name
			).str()
		) ;
		
		m_outputter.connection().run_statement(
			(
				boost::format(
					"CREATE VIEW IF NOT EXISTS %sView AS "
					"SELECT          V.id AS variant_id, V.chromosome, V.position, V.rsid, "
					"CASE WHEN length( V.alleleA < 10 ) THEN V.alleleA ELSE substr( V.alleleA, 1, 10 ) || '...' END AS alleleA, "
					"CASE WHEN length( V.alleleB < 10 ) THEN V.alleleB ELSE substr( V.alleleB, 1, 10 ) || '...' END AS alleleB, "
					"SD.analysis_id, Analysis.name AS analysis, Variable.id AS variable_id, Variable.name AS variable, "
					"SD.value AS value "
					"FROM SummaryData SD "
					"INNER JOIN Variant V ON V.id == SD.variant_id "
					"INNER JOIN Analysis ON Analysis.id = SD.analysis_id "
					"INNER JOIN Entity Variable ON Variable.id = SD.variable_id "
					"WHERE analysis_id IN ( SELECT id FROM Entity )"
				) % m_table_name
			).str()
		) ;
		
		m_insert_data_statement = m_outputter.connection().get_statement(
			(
				boost::format(
					"INSERT INTO %s ( variant_id, analysis_id, variable_id, value ) "
					"VALUES( ?1, ?2, ?3, ?4 )"
				) % m_table_name
			).str()
		) ;
		
	}

	void DBOutputter::write_data( Data const& data ) {
		db::Connection::ScopedTransactionPtr transaction = m_outputter.connection().open_transaction( 2400 ) ; // wait 10 minutes if we have to.

		if( !transaction.get() ) {
			throw genfile::OperationFailedError( "SNPSummaryComponent::DBOutputter::write_data()", m_outputter.connection().get_spec(), "Opening transaction." ) ;
		}

		if( !m_insert_data_statement.get() ) {
			create_schema() ;
		}
		
		std::vector< db::Connection::RowId > snp_ids( data.size() ) ;
		for( std::size_t i = 0; i < data.size(); ++i ) {
			// Common usage is to record many variables for each SNP, so optimise for that usage.
			// i.e. only try to make a new variant if it differs from the previous one.
			if( i == 0 || data[i].get<0>() != data[i-1].get<0>() ) {
				snp_ids[i] = m_outputter.get_or_create_variant( data[i].get<0>() ) ;
			}
			else {
				snp_ids[i] = snp_ids[i-1] ;
			}
		}
		for( std::size_t i = 0; i < data.size(); ++i ) {
			store_data(
				snp_ids[i],
				data[i].get<1>(),
				data[i].get<2>()
			) ;
		}
	}

	void DBOutputter::store_data(
		db::Connection::RowId const snp_id,
		std::string const& variable,
		genfile::VariantEntry const& value
	) {
		db::Connection::RowId variable_id = m_outputter.get_or_create_entity( variable, variable, m_variable_id ) ;
		insert_summary_data( snp_id, variable_id, value ) ;
	}
	
	void DBOutputter::insert_summary_data(
		db::Connection::RowId snp_id,
		db::Connection::RowId variable_id,
		genfile::VariantEntry const& value
	) const {
		m_insert_data_statement
			->bind( 1, snp_id )
			.bind( 2, m_outputter.analysis_id() )
			.bind( 3, variable_id )
			.bind( 4, value )
			.step() ;
		m_insert_data_statement->reset() ;
	}
	
}
