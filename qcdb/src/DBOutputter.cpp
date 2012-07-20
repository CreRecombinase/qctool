
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
#include "../../qctool_version_autogenerated.hpp"

namespace qcdb {
	DBOutputter::UniquePtr DBOutputter::create( std::string const& filename, std::string const& cohort_name, Metadata const& metadata ) {
		return UniquePtr( new DBOutputter( filename, cohort_name, metadata ) ) ;
	}
	DBOutputter::SharedPtr DBOutputter::create_shared( std::string const& filename, std::string const& cohort_name, Metadata const& metadata ) {
		return SharedPtr( new DBOutputter( filename, cohort_name, metadata ) ) ;
	}

	DBOutputter::~DBOutputter() {}

	DBOutputter::DBOutputter( std::string const& filename, std::string const& cohort_name, Metadata const& metadata ):
		m_connection( db::Connection::create( filename )),
		m_analysis_name( cohort_name ),
		m_metadata( metadata )
	{
		// Increase cache size to be about 104Mb=102,400kb for performance reasons.
		db::Connection::ScopedTransactionPtr transaction = m_connection->open_transaction( 120 ) ; // wait 2m if we have to.
		m_connection->run_statement( "PRAGMA cache_size=-102400" ) ;
		m_connection->run_statement( "PRAGMA synchronous=OFF" ) ;
		m_connection->run_statement(
			"CREATE TABLE IF NOT EXISTS Variant ( id INTEGER PRIMARY KEY, snpid TEXT, rsid TEXT, chromosome TEXT, position INTEGER, alleleA TEXT, alleleB TEXT )"
		) ;
		m_connection->run_statement(
			"CREATE INDEX IF NOT EXISTS Variant_index ON Variant( chromosome, position )"
		) ;
		m_connection->run_statement(
			"CREATE INDEX IF NOT EXISTS Variant_rsid_index ON Variant( rsid )"
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
			"CREATE TABLE IF NOT EXISTS SummaryData ( "
			"variant_id INT, analysis_id INT, variable_id INT, value NONE, "
			"FOREIGN KEY( variant_id ) REFERENCES Variant( id ), "
			"FOREIGN KEY( analysis_id ) REFERENCES Entity( id ), "
			"FOREIGN KEY( variable_id ) REFERENCES Entity( id ) 	"
			")"
		) ;
		m_connection->run_statement(
			"CREATE INDEX IF NOT EXISTS SummaryDataIndex ON SummaryData( analysis_id, variant_id, variable_id )"
		) ;
		m_connection->run_statement(
			"CREATE INDEX IF NOT EXISTS EntityDataIndex ON EntityData( entity_id, variable_id )"
		) ;

		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS EntityDataView AS "
			"SELECT ED.entity_id, E.name, ED.variable_id, V.name, ED.value "
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
			"CREATE VIEW IF NOT EXISTS AnalysisListView AS "
			"SELECT ERV.* "
			"FROM EntityRelationshipView ERV "
			"WHERE ERV.relationship == 'is_a' AND ERV.entity2 == 'analysis'"
		) ;
	
		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS SummaryDataView AS "
			"SELECT          V.id AS variant_id, V.chromosome, V.position, V.rsid, "
			"SD.analysis_id, Analysis.name AS analysis, Variable.id AS variable_id, Variable.name AS variable, "
			"SD.value AS value "
			"FROM SummaryData SD "
			"INNER JOIN Variant V ON V.id == SD.variant_id "
			"INNER JOIN Entity Analysis ON Analysis.id = SD.analysis_id "
			"INNER JOIN Entity Variable ON Variable.id = SD.variable_id"
		) ;
		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS SNPFilterView AS "
			"SELECT          Analysis.id AS Analysis_id, Analysis.name AS analysis, V.id AS variant_id, V.chromosome, V.position, V.rsid, "
			"AA.value AS 'AA', "
			"AB.value AS 'AB', "
			"BB.value AS 'BB', "
			"MAF.value AS 'MAF', "
			"HWE.value AS 'minus_log10_exact_HW_p_value', "
			"Missing.value AS 'missing_proportion', "
			"Info.value AS 'impute_info' "
			"FROM            Variant V "
			"INNER JOIN      Entity Analysis "
			"LEFT OUTER JOIN      SummaryData AA "
			"ON          AA.variable_id IN ( SELECT id FROM Entity WHERE name = 'AA' ) "
			"AND         AA.analysis_id = Analysis.id "
			"AND         AA.variant_id == V.id "
			"LEFT OUTER JOIN      SummaryData AB "
			"ON          AB.variable_id IN ( SELECT id FROM Entity WHERE name = 'AB' ) "
			"AND         AB.analysis_id = Analysis.id "
			"AND         AB.variant_id == V.id "
			"LEFT OUTER JOIN      SummaryData BB "
			"ON          BB.variable_id IN ( SELECT id FROM Entity WHERE name = 'BB' ) "
			"AND         BB.analysis_id = Analysis.id "
			"AND         BB.variant_id == V.id "
			"LEFT OUTER JOIN      SummaryData Missing "
			"ON          Missing.variable_id IN ( SELECT id FROM Entity WHERE name = 'missing proportion' ) "
			"AND         Missing.analysis_id = Analysis.id "
			"AND         Missing.variant_id == V.id "
			"LEFT OUTER JOIN      SummaryData MAF "
			"ON          MAF.variable_id = ( SELECT id FROM Entity WHERE name = 'minor_allele_frequency' ) "
			"AND         MAF.analysis_id = Analysis.id "
			"AND         MAF.variant_id == V.id "
			"LEFT OUTER JOIN      SummaryData HWE "
			"ON          HWE.variant_id == V.id "
			"AND         HWE.analysis_id = Analysis.id "
			"AND         HWE.variable_id == ( SELECT id FROM Entity WHERE name == 'minus_log10_exact_HW_p_value' ) "
			"LEFT OUTER JOIN      SummaryData Info "
			"ON          Info.variant_id == V.id "
			"AND         Info.analysis_id = Analysis.id "
			"AND         Info.variable_id == ( SELECT id FROM Entity WHERE name == 'impute_info' ) "
			"WHERE       EXISTS( SELECT * FROM SummaryData WHERE analysis_id == Analysis.id ) "
		) ;
		
		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS SNPTestView AS "
			"SELECT        Analysis.id AS analysis_id, Analysis.name AS analysis, V.id AS variant_id, V.chromosome, V.position, V.rsid, "
			"Beta.value AS beta, "
			"Se.value AS se, "
			"Pvalue.value AS pvalue "
			"FROM          Variant V "
			"INNER JOIN    Entity Analysis "
			"LEFT OUTER JOIN SummaryData Beta "
			"ON       Beta.variable_id IN ( SELECT id FROM Entity WHERE name == 'beta_1' ) "
			"AND      Beta.analysis_id = Analysis.id "
			"AND      Beta.variant_id = V.id "
			"LEFT OUTER JOIN SummaryData SE "
			"ON       SE.variable_id IN ( SELECT id FROM Entity WHERE name == 'se_1' ) "
			"AND      SE.analysis_id = Analysis.id "
			"AND      SE.variant_id = V.id "
			"LEFT OUTER JOIN SummaryData Pvalue "
			"ON       Pvalue.variable_id IN ( SELECT id FROM Entity WHERE name == 'p_value' ) "
			"AND      Pvalue.analysis_id = Analysis.id "
			"AND      Pvalue.variant_id = V.id "
			"WHERE       EXISTS( SELECT * FROM SummaryData WHERE analysis_id == Analysis.id ) "
		) ;
		
		m_connection->run_statement(
			"CREATE VIEW IF NOT EXISTS AlleleView AS "
			"SELECT          A.id AS analysis_id, A.name AS analysis, "
			"                V.id AS variant_id, V.chromosome, V.position, V.rsid, V.alleleA, V.alleleB, "
			"                BF.value AS alleleB_frequency, "
			"                AA.value AS ancestral_allele, "
			"                DA.value AS derived_allele, "
			"                DAF.value AS derived_allele_frequency "
			"FROM            Variant V "
			"INNER JOIN      Entity A "
			"INNER JOIN SummaryData BF "
			"    ON          BF.analysis_id = A.id "
			"    AND         BF.variant_id = V.id "
			"    AND         BF.variable_id IN ( SELECT id FROM Entity WHERE name == 'alleleB_frequency' ) "
			"LEFT OUTER JOIN SummaryData AA "
			"    ON          AA.analysis_id = A.id "
			"    AND         AA.variant_id = V.id "
			"    AND         AA.variable_id IN ( SELECT id FROM Entity WHERE name == 'ancestral_allele' ) "
			"LEFT OUTER JOIN SummaryData DA "
			"    ON          DA.analysis_id = A.id "
			"    AND         DA.variant_id = V.id "
			"    AND         DA.variable_id IN ( SELECT id FROM Entity WHERE name == 'derived_allele' ) "
			"LEFT OUTER JOIN SummaryData DAF "
			"    ON          DAF.analysis_id = A.id "
			"    AND         DAF.variant_id = V.id "
			"    AND         DAF.variable_id IN ( SELECT id FROM Entity WHERE name == 'derived_allele_frequency' ) "
		) ;
		
		construct_statements() ;
		store_metadata() ;
	}

	void DBOutputter::construct_statements() {
		m_insert_entity_statement = m_connection->get_statement( "INSERT INTO Entity ( name, description ) VALUES ( ?1, ?2 )" ) ;
		m_find_entity_data_statement = m_connection->get_statement( "SELECT * FROM EntityData WHERE entity_id == ?1 AND variable_id == ?2" ) ;
		m_find_entity_statement = m_connection->get_statement( "SELECT id FROM Entity E WHERE name == ?1 AND description == ?2" ) ;
		m_insert_entity_data_statement = m_connection->get_statement( "INSERT OR REPLACE INTO EntityData ( entity_id, variable_id, value ) VALUES ( ?1, ?2, ?3 )" ) ;
		m_insert_entity_relationship_statement = m_connection->get_statement( "INSERT OR REPLACE INTO EntityRelationship( entity1_id, relationship_id, entity2_id ) VALUES( ?1, ?2, ?3 )") ;
		m_find_variant_statement = connection().get_statement(
			"SELECT id FROM Variant WHERE chromosome == ?2 AND position == ?3 AND rsid == ?1 AND alleleA = ?4 AND alleleB = ?5"
		) ;
		m_insert_variant_statement = connection().get_statement(
			"INSERT INTO Variant ( snpid, rsid, chromosome, position, alleleA, alleleB ) "
			"VALUES( ?1, ?2, ?3, ?4, ?5, ?6 )"
		) ;
		m_insert_summarydata_statement = m_connection->get_statement(
			"INSERT INTO SummaryData ( variant_id, analysis_id, variable_id, value ) "
			"VALUES( ?1, ?2, ?3, ?4 )"
		) ;
	}

	void DBOutputter::store_metadata() {
		load_entities() ;
		m_is_a = get_or_create_entity_internal( "is_a", "is_a relationship" ) ;
		m_used_by = get_or_create_entity_internal( "used_by", "used_by relationship" ) ;
		m_analysis = get_or_create_entity_internal( "analysis", "class of analyses" ) ;
		db::Connection::RowId const cmd_line_arg_id = get_or_create_entity_internal( "command-line argument", "Value supplied to a script or executable" ) ;

		m_analysis_id = get_or_create_entity_internal(
			m_analysis_name,
			"qctool analysis, started " + appcontext::get_current_time_as_string(),
			m_analysis
		) ;

		get_or_create_entity_data(
			m_analysis_id,
			get_or_create_entity( "tool", "Executable, pipeline, or script used to generate these results." ),
			"qctool revision " + std::string( globals::qctool_revision )
		) ;
		
		for( Metadata::const_iterator i = m_metadata.begin(); i != m_metadata.end(); ++i ) {
			db::Connection::RowId key_id = get_or_create_entity( i->first, "command-line argument", cmd_line_arg_id ) ;
			get_or_create_entity_data(
				m_analysis_id,
				key_id,
				genfile::string_utils::join( i->second.first, "," ) + " (" + i->second.second + ")"
			) ;
		}
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
		}
		return result ;
	}

	db::Connection::RowId DBOutputter::get_or_create_entity( std::string const& name, std::string const& description, boost::optional< db::Connection::RowId > class_id ) const {
		db::Connection::RowId result ;
		EntityMap::const_iterator where = m_entity_map.find( std::make_pair( name, description )) ;
		if( where != m_entity_map.end() ) {
			result = where->second ;
		}
		else {
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
			create_entity_relationship( result, m_used_by, m_analysis_id ) ;
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

	db::Connection::RowId DBOutputter::get_or_create_variant( genfile::SNPIdentifyingData const& snp ) const {
		db::Connection::RowId result ;
		m_find_variant_statement
			->bind( 1, snp.get_rsid() )
			.bind( 2, std::string( snp.get_position().chromosome() ))
			.bind( 3, snp.get_position().position() )
			.bind( 4, snp.get_first_allele() )
			.bind( 5, snp.get_second_allele() )
			.step()
		;
		if( m_find_variant_statement->empty() ) {
			m_insert_variant_statement
				->bind( 1, snp.get_SNPID() )
				.bind( 2, snp.get_rsid() )
				.bind( 3, std::string( snp.get_position().chromosome() ) )
				.bind( 4, snp.get_position().position() )
				.bind( 5, snp.get_first_allele())
				.bind( 6, snp.get_second_allele())
				.step()
			;

			result = connection().get_last_insert_row_id() ;
			m_insert_variant_statement->reset() ;
		} else {
			result = m_find_variant_statement->get< db::Connection::RowId >( 0 ) ;
		}
		m_find_variant_statement->reset() ;
		return result ;
	}
	
	void DBOutputter::insert_summary_data(
		db::Connection::RowId snp_id,
		db::Connection::RowId variable_id,
		genfile::VariantEntry const& value
	) const {
		m_insert_summarydata_statement
			->bind( 1, snp_id )
			.bind( 2, m_analysis_id )
			.bind( 3, variable_id )
			.bind( 4, value )
			.step() ;
		m_insert_summarydata_statement->reset() ;
	}
}
