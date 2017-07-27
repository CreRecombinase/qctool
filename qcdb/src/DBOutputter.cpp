
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <memory>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread/thread.hpp>
#include "genfile/SNPIdentifyingData.hpp"
#include "genfile/VariantEntry.hpp"
#include "genfile/Error.hpp"
#include "db/Connection.hpp"
#include "db/SQLStatement.hpp"
#include "appcontext/get_current_time_as_string.hpp"
#include "qcdb/DBOutputter.hpp"

namespace qcdb {
	namespace impl {
		bool get_match_rsid( std::string spec ) {
			std::vector< std::string > elts = genfile::string_utils::split_and_strip( spec, "," ) ;

			if(
				std::find( elts.begin(), elts.end(), "position" ) == elts.end()
				|| std::find( elts.begin(), elts.end(), "alleles" ) == elts.end()
			) {
				throw genfile::BadArgumentError(
					"qcdb::impl::get_match_rsid()",
					"spec=\"" + spec + "\"",
					"Spec must include position and alleles."
				) ;
			}
			
			return std::find( elts.begin(), elts.end(), "rsid" ) != elts.end() ;
		}
	}

	DBOutputter::UniquePtr DBOutputter::create(
		std::string const& filename,
		std::string const& analysis_name,
		std::string const& analysis_description,
		Metadata const& metadata,
		boost::optional< db::Connection::RowId > analysis_id,
		std::string const& snp_match_fields
	) {
		return UniquePtr( new DBOutputter( filename, analysis_name, analysis_description, metadata, analysis_id, snp_match_fields ) ) ;
	}
	DBOutputter::SharedPtr DBOutputter::create_shared(
		std::string const& filename,
		std::string const& analysis_name,
		std::string const& analysis_description,
		Metadata const& metadata,
		boost::optional< db::Connection::RowId > analysis_id,
		std::string const& snp_match_fields
	) {
		return SharedPtr( new DBOutputter( filename, analysis_name, analysis_description, metadata, analysis_id, snp_match_fields ) ) ;
	}

	DBOutputter::~DBOutputter() {}

	DBOutputter::DBOutputter(
		std::string const& filename,
		std::string const& analysis_name,
		std::string const& analysis_description,
		Metadata const& metadata,
		boost::optional< db::Connection::RowId > analysis_id,
		std::string const& snp_match_fields
	):
		m_connection( db::Connection::create( filename )),
		m_analysis_name( analysis_name ),
		m_analysis_chunk( analysis_description ),
		m_metadata( metadata ),
		m_create_indices( true ),
		m_match_rsid( impl::get_match_rsid( snp_match_fields )),
		m_analysis_id( analysis_id )
	{
		try {
			m_connection->run_statement( "PRAGMA journal_mode = OFF" ) ;
			m_connection->run_statement( "PRAGMA synchronous = OFF" ) ;
		}
		catch( db::Error const& ) {
			std::cerr << "qcdb::DBOutputter::DBOutputter(): unable to set PRAGMA synchronous=OFF, is another connection using this database?" ;
		}

		db::Connection::ScopedTransactionPtr transaction = m_connection->open_transaction( 7200 ) ;

		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS Variant ( id INTEGER PRIMARY KEY, rsid TEXT, chromosome TEXT, position INTEGER, alleleA TEXT, alleleB TEXT )"
		) ;
		m_connection->run_statement(
			"CREATE INDEX IF NOT EXISTS Variant_position_index ON Variant( chromosome, position )"
		) ;
		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS VariantIdentifier ( variant_id INTEGER NOT NULL, identifier TEXT, FOREIGN KEY( variant_id ) REFERENCES Variant( id ) ) "
		) ;
		m_connection->run_statement(
			"CREATE INDEX IF NOT EXISTS VariantIdentifierIdentifierIndex ON VariantIdentifier( identifier )"
		) ;
		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS Entity ( "
				"id INTEGER PRIMARY KEY, name TEXT, description TEXT, "
				"UNIQUE( name, description ) "
			")"
		) ;
		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS EntityData ( "
			"entity_id INTEGER NOT NULL, "
			"variable_id INTEGER NOT NULL, "
			"value TEXT, "
			"FOREIGN KEY (entity_id) REFERENCES Entity( id ), "
			"FOREIGN KEY (variable_id) REFERENCES Entity( id ) "
			")"
		) ;
		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS EntityRelationship ( "
				"entity1_id INTEGER NOT NULL, "
				"relationship_id INTEGER NOT NULL, "
				"entity2_id INTEGER NOT NULL, "
				"FOREIGN KEY( entity1_id ) REFERENCES Entity( id ), "
				"FOREIGN KEY( relationship_id ) REFERENCES Entity( id ), "
				"FOREIGN KEY( entity2_id ) REFERENCES Entity( id ), "
				"UNIQUE( entity1_id, relationship_id, entity2_id ) "
			")"
		) ;

		m_connection->run_statement(
			"CREATE INDEX IF NOT EXISTS EntityDataIndex ON EntityData( entity_id, variable_id )"
		) ;

		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS EntityDataView AS "
			"SELECT ED.entity_id, E.name, ED.variable_id, V.name AS variable, ED.value "
			"FROM EntityData ED "
			"INNER JOIN Entity E "
			"ON E.id = ED.entity_id "
			"INNER JOIN Entity V "
			"ON V.id = ED.variable_id"
		) ;

		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS EntityRelationshipView AS "
			"SELECT ER.entity1_id AS entity1_id, E1.name AS entity1, ER.relationship_id, R.name AS relationship, ER.entity2_id AS entity2_id, E2.name AS entity2 "
			"FROM EntityRelationship ER "
			"INNER JOIN Entity E1 "
			"  ON E1.id = ER.entity1_id " 
			"INNER JOIN Entity R "
			"  ON R.id = ER.relationship_id " 
			"INNER JOIN Entity E2 "
			"  ON E2.id = ER.entity2_id" 
		) ;

		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS Analysis ( "
				"id INTEGER PRIMARY KEY, "
				"name TEXT, "
				"chunk TEXT"
			")"
		) ;
		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS AnalysisProperty( "
			"analysis_id INTEGER NOT NULL REFERENCES Analysis( id ), "
			"property TEXT NOT NULL, "
			"value TEXT, "
			"source TEXT"
			")"
		) ;
		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS AnalysisPropertyView AS "
			"SELECT analysis_id, name, property, value "
			"FROM AnalysisProperty AP "
			"INNER JOIN Analysis A "
			"ON A.id = AP.analysis_id"
		) ;
		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS AnalysisStatus ( "
				"analysis_id INTEGER NOT NULL REFERENCES Analysis( id ), "
				"started TEXT NOT NULL, "
				"completed TEXT, "
				"status TEXT NOT NULL "
			")"
		) ;
		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS AnalysisStatusView AS "
			"SELECT analysis_id, name AS analysis, chunk, started, completed, status "
			"FROM AnalysisStatus AST "
			"INNER JOIN Analysis A "
			"ON A.id == AST.analysis_id"
		) ;
		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS VariantView AS "
			"SELECT          V.id AS id, V.rsid AS rsid, V.chromosome AS chromosome, V.position AS position, V.alleleA AS alleleA, V.alleleB AS alleleB, "
			"GROUP_CONCAT( VI.identifier ) AS alternate_identifier "
			"FROM Variant V "
			"LEFT OUTER JOIN VariantIdentifier VI "
			"  ON VI.variant_id = V.id "
			"GROUP BY V.id"
		) ;
		construct_statements() ;
		store_metadata() ;
	}

	void DBOutputter::finalise( long options ) {
		if( options & eCreateIndices ) {
			db::Connection::ScopedTransactionPtr transaction = m_connection->open_transaction( 7200 ) ;
			m_connection->run_statement(
				"CREATE INDEX IF NOT EXISTS Variant_rsid_index ON Variant( rsid )"
			) ;
			m_connection->run_statement(
				"CREATE INDEX IF NOT EXISTS VariantIdentifierVariantIndex ON VariantIdentifier( variant_id )"
			) ;
		}
		db::Connection::ScopedTransactionPtr transaction = m_connection->open_transaction( 7200 ) ;
		end_analysis( m_analysis_id.get() ) ;
	}

	void DBOutputter::construct_statements() {
		m_insert_entity_statement = m_connection->get_statement( "INSERT INTO Entity ( name, description ) VALUES ( ?1, ?2 )" ) ;
		m_find_entity_data_statement = m_connection->get_statement( "SELECT * FROM EntityData WHERE entity_id == ?1 AND variable_id == ?2" ) ;
		m_find_entity_statement = m_connection->get_statement( "SELECT id FROM Entity E WHERE name == ?1 AND description == ?2" ) ;
		m_insert_entity_data_statement = m_connection->get_statement( "INSERT OR REPLACE INTO EntityData ( entity_id, variable_id, value ) VALUES ( ?1, ?2, ?3 )" ) ;

		m_find_analysis_statement = m_connection->get_statement( "SELECT id FROM Analysis WHERE name == ?1 AND chunk == ?2" ) ;
		m_insert_analysis_statement = m_connection->get_statement( "INSERT INTO Analysis( name, chunk ) VALUES ( ?1, ?2 )" ) ;
		m_insert_analysis_property_statement = m_connection->get_statement( "INSERT OR REPLACE INTO AnalysisProperty ( analysis_id, property, value, source ) VALUES ( ?1, ?2, ?3, ?4 )" ) ;

		m_insert_entity_relationship_statement = m_connection->get_statement( "INSERT OR REPLACE INTO EntityRelationship( entity1_id, relationship_id, entity2_id ) VALUES( ?1, ?2, ?3 )") ;

		{
			std::string find_variant_statement_sql = "SELECT id, rsid FROM Variant WHERE IFNULL( chromosome, '.' ) == ?1 AND position == ?2 AND alleleA = ?3 AND alleleB = ?4" ;
			if( m_match_rsid ) {
				find_variant_statement_sql += " AND rsid == ?5" ;
			}
			m_find_variant_statement = connection().get_statement( find_variant_statement_sql ) ;
		}

		m_insert_variant_statement = connection().get_statement(
			"INSERT INTO Variant ( rsid, chromosome, position, alleleA, alleleB ) "
			"VALUES( ?1, ?2, ?3, ?4, ?5 )"
		) ;
		m_find_variant_identifier_statement = m_connection->get_statement( "SELECT * FROM VariantIdentifier WHERE variant_id == ?1 AND identifier == ?2" ) ;
		m_insert_variant_identifier_statement = m_connection->get_statement( "INSERT INTO VariantIdentifier( variant_id, identifier ) VALUES ( ?1, ?2 )" ) ;
	}

	void DBOutputter::store_metadata() {
		load_entities() ;
		m_is_a = get_or_create_entity_internal( "is_a", "is_a relationship" ) ;
		m_used_by = get_or_create_entity_internal( "used_by", "used_by relationship" ) ;

		try {
			if( m_analysis_id ) {
				m_find_analysis_statement
					->bind( 1, m_analysis_name )
					.bind( 2, m_analysis_chunk )
					.step() ;
				if( m_find_analysis_statement->empty() ) {
					throw genfile::BadArgumentError(
						"qcdb::DBOutputter::store_metadata()",
						"m_analysis_id=" + genfile::string_utils::to_string( m_analysis_id.get() ),
						"Could not find an analysis with the given name and chunk."
					) ;
				}
				db::Connection::RowId const id = m_find_analysis_statement->get< db::Connection::RowId >( 0 ) ;
				if( id != m_analysis_id.get() ) {
					throw genfile::BadArgumentError(
						"qcdb::DBOutputter::store_metadata()",
						"m_analysis_id=" + genfile::string_utils::to_string( m_analysis_id.get() ),
						"id does not match the analysis with with the given name and chunk."
					) ;
				}
				m_find_analysis_statement->reset() ;
			} else {
				m_analysis_id = create_analysis(
					m_analysis_name,
					m_analysis_chunk
				) ;
			}
		} catch( db::StatementStepError const& e ) {
			throw genfile::BadArgumentError( "qcdb::DBOutputter::store_metadata()", "analysis_name=\"" + m_analysis_name + "\"", "An analysis with name \"" + m_analysis_name + "\" and chunk \"" + m_analysis_chunk + "\" already exists" ) ;
		}

		start_analysis( m_analysis_id.get() ) ;

		for( Metadata::const_iterator i = m_metadata.begin(); i != m_metadata.end(); ++i ) {
			set_analysis_property(
				m_analysis_id.get(),
				i->first,
				genfile::string_utils::join( i->second.first, "," ),
				i->second.second
			) ;
		}
	}
	
	void DBOutputter::start_analysis( db::Connection::RowId const analysis_id ) const {
		db::Connection::StatementPtr stmnt = m_connection->get_statement( "INSERT INTO AnalysisStatus( analysis_id, started, status ) VALUES( ?, ?, ? )" ) ;
		stmnt->bind( 1, analysis_id ) ;
		stmnt->bind( 2, appcontext::get_current_time_as_string() ) ;
		stmnt->bind( 3, "incomplete" ) ;
		stmnt->step() ;
	}

	void DBOutputter::end_analysis( db::Connection::RowId const analysis_id ) const {
		db::Connection::StatementPtr stmnt = m_connection->get_statement( "UPDATE AnalysisStatus SET completed = ?, status = ? WHERE analysis_id == ?" ) ;
		stmnt->bind( 1, appcontext::get_current_time_as_string() ) ;
		stmnt->bind( 2, "success" ) ;
		stmnt->bind( 3, analysis_id ) ;
		stmnt->step() ;
	}
	
	void DBOutputter::load_entities() {
		db::Connection::StatementPtr stmnt = m_connection->get_statement( "SELECT id, name, description FROM Entity" ) ;
		stmnt->step() ;
		while( !stmnt->empty() ) {
			m_entity_map.insert( std::make_pair( std::make_pair( stmnt->get< std::string >( 1 ), stmnt->get< std::string >( 2 )), stmnt->get< db::Connection::RowId >( 0 ))) ;
			stmnt->step() ;
		}
		stmnt->reset() ;
	}

	db::Connection::RowId DBOutputter::get_or_create_entity_internal( std::string const& name, std::string const& description, boost::optional< db::Connection::RowId > class_id ) const {
		db::Connection::RowId result ;
		EntityMap::const_iterator where = m_entity_map.find( std::make_pair( name, description )) ;
		if( where != m_entity_map.end() ) {
			result = where->second ;
		}
		else {
			m_find_entity_statement
				->bind( 1, name )
				.bind( 2, description )
				.step() ;
			if( m_find_entity_statement->empty() ) {
				result = create_entity_internal( name, description, class_id ) ;
			} else {
				result = m_find_entity_statement->get< db::Connection::RowId >( 0 ) ;
			}
			m_find_entity_statement->reset() ;
		}
		return result ;
	}
	
	db::Connection::RowId DBOutputter::create_entity_internal( std::string const& name, std::string const& description, boost::optional< db::Connection::RowId > class_id ) const {
		db::Connection::RowId result ;
		m_insert_entity_statement
			->bind( 1, name )
			.bind( 2, description )
			.step() ;
			
		result = m_connection->get_last_insert_row_id() ;
		m_entity_map.insert( std::make_pair( std::make_pair( name, description ), result ) ) ;
		m_insert_entity_statement->reset() ;

		if( class_id ) {
			create_entity_relationship( result, m_is_a, *class_id ) ;
		}
		return result ;
	}

	db::Connection::RowId DBOutputter::create_analysis( std::string const& name, std::string const& description ) const {
		db::Connection::RowId result ;
		m_insert_analysis_statement
			->bind( 1, name )
			.bind( 2, description )
			.step() ;
			
		result = m_connection->get_last_insert_row_id() ;
		m_insert_analysis_statement->reset() ;

		return result ;
	}

	db::Connection::RowId DBOutputter::get_or_create_entity( std::string const& name, std::string const& description, boost::optional< db::Connection::RowId > class_id ) const {
		db::Connection::RowId result ;
		EntityMap::const_iterator where = m_entity_map.find( std::make_pair( name, description )) ;
		if( where != m_entity_map.end() ) {
			result = where->second ;
		}
		else {
			m_find_entity_statement
				->bind( 1, name )
				.bind( 2, description )
				.step() ;
			if( m_find_entity_statement->empty() ) {
				result = create_entity_internal( name, description, class_id ) ;
				create_entity_relationship( result, m_used_by, m_analysis_id.get() ) ;
			} else {
				result = m_find_entity_statement->get< db::Connection::RowId >( 0 ) ;
			}
			m_find_entity_statement->reset() ;
		}
		return result ;
	}

	void DBOutputter::create_entity_relationship( db::Connection::RowId entity1_id, db::Connection::RowId relationship_id, db::Connection::RowId entity2_id ) const {
		m_insert_entity_relationship_statement
			->bind( 1, entity1_id )
			.bind( 2, relationship_id )
			.bind( 3, entity2_id )
			.step() ;
			
		m_insert_entity_relationship_statement->reset() ;
	}

	db::Connection::RowId DBOutputter::get_or_create_entity_data( db::Connection::RowId const entity_id, db::Connection::RowId const variable_id, genfile::VariantEntry const& value ) const {
		db::Connection::RowId result ;

		m_find_entity_data_statement
			->bind( 1, entity_id )
			.bind( 2, variable_id ).step() ;

		if( m_find_entity_data_statement->empty() ) {
			m_insert_entity_data_statement
				->bind( 1, entity_id )
				.bind( 2, variable_id )
				.bind( 3, value )
				.step() ;
			result = m_connection->get_last_insert_row_id() ;
			m_insert_entity_data_statement->reset() ;
		} else {
			result = m_find_entity_data_statement->get< db::Connection::RowId >( 0 ) ;
		}
		m_find_entity_data_statement->reset() ;
		return result ;
	}

	db::Connection::RowId DBOutputter::set_analysis_property(
		db::Connection::RowId const analysis_id,
		std::string const& property,
		genfile::VariantEntry const& value,
		std::string const& aux
	) const {
		db::Connection::RowId result ;

		m_insert_analysis_property_statement
			->bind( 1, analysis_id )
			.bind( 2, property )
			.bind( 3, value )
			.bind( 4, aux )
			.step() ;
		result = m_connection->get_last_insert_row_id() ;
		m_insert_analysis_property_statement->reset() ;
		return result ;
	}

	void DBOutputter::add_alternative_variant_identifier( db::Connection::RowId const variant_id, std::string const& identifier, std::string const& rsid ) const {
		if( identifier != rsid  && identifier != "---" && identifier != "." ) {
			add_variant_identifier( variant_id, identifier ) ;
		}
	}

	void DBOutputter::add_variant_identifier( db::Connection::RowId const variant_id, std::string const& identifier ) const {
		m_find_variant_identifier_statement
			->bind( 1, variant_id )
			.bind( 2, identifier )
			.step() ;
		if( m_find_variant_identifier_statement->empty() ) {
			m_insert_variant_identifier_statement
				->bind( 1, variant_id )
				.bind( 2, identifier )
				.step() ;
			m_insert_variant_identifier_statement->reset() ;
		}
		m_find_variant_identifier_statement->reset() ;
	}

	db::Connection::RowId DBOutputter::get_or_create_variant( genfile::VariantIdentifyingData const& snp ) const {
		db::Connection::RowId result ;
		if( snp.get_position().chromosome().is_missing() ) {
			// Find variant statement uses IFNULL to replace NULLs with a . for comparison.
			m_find_variant_statement->bind( 1, "." ) ;
		} else {
			m_find_variant_statement
				->bind( 1, std::string( snp.get_position().chromosome() ) ) ;
		}
		m_find_variant_statement
			->bind( 2, snp.get_position().position() )
			.bind( 3, snp.get_allele(0) )
			.bind( 4, snp.get_allele(1) ) ;
		if( m_match_rsid ) {
			m_find_variant_statement->bind( 5, snp.get_primary_id() ) ;
		}
		m_find_variant_statement->step() ;

		if( m_find_variant_statement->empty() ) {
			if( snp.get_position().chromosome().is_missing() ) {
				m_insert_variant_statement
					->bind_NULL( 2 ) ;
			} else {
				m_insert_variant_statement
					->bind( 2, std::string( snp.get_position().chromosome() ) ) ;
			}

			m_insert_variant_statement
				->bind( 1, snp.get_primary_id() )
				.bind( 3, snp.get_position().position() )
				.bind( 4, snp.get_allele(0))
				.bind( 5, snp.get_allele(1))
				.step()
			;

			result = connection().get_last_insert_row_id() ;
			m_insert_variant_statement->reset() ;
			snp.get_identifiers(
				boost::bind(
					&DBOutputter::add_alternative_variant_identifier,
					this,
					result,
					_1,
					snp.get_primary_id()
				),
				1
			) ;
		} else {
			result = m_find_variant_statement->get< db::Connection::RowId >( 0 ) ;
			std::string const rsid = m_find_variant_statement->get< std::string >( 1 ) ;
			add_alternative_variant_identifier( result, snp.get_primary_id(), rsid ) ;
			snp.get_identifiers(
				boost::bind(
					&DBOutputter::add_alternative_variant_identifier,
					this,
					result,
					_1,
					rsid
				),
				1
			) ;
		}
		m_find_variant_statement->reset() ;
		return result ;
	}
	
}
