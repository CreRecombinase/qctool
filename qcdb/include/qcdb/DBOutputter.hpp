
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef QCTOOL_QCDB_DB_OUTPUTTER_HPP
#define QCTOOL_QCDB_DB_OUTPUTTER_HPP

#include <string>
#include <memory>
#include <map>
#include <utility>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/unordered_map.hpp>
#include "genfile/VariantIdentifyingData.hpp"
#include "genfile/CohortIndividualSource.hpp"
#include "genfile/VariantEntry.hpp"
#include "db/Connection.hpp"
#include "db/SQLStatement.hpp"
#include "qcdb/StorageOptions.hpp"

namespace qcdb {
	struct DBOutputter {
		typedef std::auto_ptr< DBOutputter > UniquePtr ;
		typedef boost::shared_ptr< DBOutputter > SharedPtr ;
		typedef std::map< std::string, std::pair< std::vector< std::string >, std::string > > Metadata ;

		static UniquePtr create(
			std::string const& filename,
			std::string const& analysis_name,
			std::string const& analysis_description,
			Metadata const& metadata,
			boost::optional< db::Connection::RowId > analysis_id = boost::optional< db::Connection::RowId >(),
			std::string const& snp_match_fields = "position,alleles"
		) ;
		static SharedPtr create_shared(
			std::string const& filename,
			std::string const& analysis_name,
			std::string const& analysis_description,
			Metadata const& metadata,
			boost::optional< db::Connection::RowId > analysis_id = boost::optional< db::Connection::RowId >(),
			std::string const& snp_match_fields = "position,alleles"
		) ;

		DBOutputter(
			std::string const& filename,
			std::string const& analysis_name,
			std::string const& analysis_description,
			Metadata const& metadata,
			boost::optional< db::Connection::RowId > analysis_id = boost::optional< db::Connection::RowId >(),
			std::string const& snp_match_fields = "position,alleles"
		) ;
		~DBOutputter() ;

		// Create a new variable associated with the current analysis
		void create_variable( std::string const& table, std::string const& name ) const ;
#if 0
		// Create an entity.  Optionally supply a class (which must be the id of another entity.)
		db::Connection::RowId get_or_create_entity(
			std::string const& name,
			std::string const& description,
			boost::optional< db::Connection::RowId > class_id = boost::optional< db::Connection::RowId >()
		) const ;

		// Associate some data with an entity.
		db::Connection::RowId get_or_create_entity_data( db::Connection::RowId const entity_id, db::Connection::RowId const variable_id, genfile::VariantEntry const& value ) const ;
		// Create a variant
#endif
		db::Connection::RowId get_or_create_variant( genfile::VariantIdentifyingData const& snp ) const ;
		// Store some data for a variant.
		void insert_summary_data( db::Connection::RowId snp_id, db::Connection::RowId variable_id, genfile::VariantEntry const& value ) const ;

		db::Connection& connection() const { return *m_connection ; }
		db::Connection::RowId analysis_id() const { return m_analysis_id.get() ; }

		void finalise( long options = eCreateIndices ) ;
		
	private:
		db::Connection::UniquePtr m_connection ;
		std::string const m_analysis_name ;
		std::string const m_analysis_chunk ;
		Metadata const m_metadata ;
		bool const m_create_indices ;
		bool const m_match_rsid ;


		db::Connection::StatementPtr m_insert_variable_statement ;

		db::Connection::StatementPtr m_find_entity_statement ;
		db::Connection::StatementPtr m_find_entity_data_statement ;
		db::Connection::StatementPtr m_insert_entity_statement ;
		db::Connection::StatementPtr m_insert_entity_data_statement ;

		db::Connection::StatementPtr m_insert_analysis_statement ;
		db::Connection::StatementPtr m_find_analysis_statement ;
		db::Connection::StatementPtr m_insert_analysis_property_statement ;

		db::Connection::StatementPtr m_insert_entity_relationship_statement ;

		db::Connection::StatementPtr m_find_variant_statement ;
		db::Connection::StatementPtr m_insert_variant_statement ;
		db::Connection::StatementPtr m_find_variant_identifier_statement ;
		db::Connection::StatementPtr m_insert_variant_identifier_statement ;

		boost::optional< db::Connection::RowId > m_analysis_id ;
		db::Connection::RowId m_is_a ;
		db::Connection::RowId m_used_by ;

		typedef boost::unordered_map< std::pair< std::string, std::string >, db::Connection::RowId > EntityMap ;
		mutable EntityMap m_entity_map ;
	private:
		void construct_statements() ;
		void store_metadata() ;

#if 0
		void load_entities() ;
		void create_entity_relationship( db::Connection::RowId entity1_id, db::Connection::RowId relationship_id, db::Connection::RowId entity2_id ) const ;
#endif
		db::Connection::RowId create_analysis(
			std::string const& name,
			std::string const& description
		) const ;
		db::Connection::RowId set_analysis_property(
			db::Connection::RowId const analysis_id,
			std::string const& property,
			genfile::VariantEntry const& value,
			std::string const& aux
		) const ;

#if 0
		db::Connection::RowId get_or_create_entity_internal(
			std::string const& name,
			std::string const& description,
			boost::optional< db::Connection::RowId > class_id = boost::optional< db::Connection::RowId >()
		) const ;
		db::Connection::RowId create_entity_internal(
			std::string const& name,
			std::string const& description,
			boost::optional< db::Connection::RowId > class_id = boost::optional< db::Connection::RowId >()
		) const ;
#endif
		void start_analysis( db::Connection::RowId const ) const ;
		void end_analysis( db::Connection::RowId const ) const ;
		void add_alternative_variant_identifier( db::Connection::RowId const variant_id, std::string const& identifier, std::string const& rsid ) const ;
		void add_variant_identifier( db::Connection::RowId const variant_id, std::string const& identifier ) const ;
	} ;
}

#endif
