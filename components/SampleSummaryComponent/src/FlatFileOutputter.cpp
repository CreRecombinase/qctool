
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <memory>
#include <boost/bimap.hpp>
#include <boost/bind.hpp>
#include "genfile/VariantEntry.hpp"
#include "genfile/VariantIdentifyingData.hpp"
#include "genfile/Error.hpp"
#include "genfile/string_utils.hpp"
#include "statfile/BuiltInTypeStatSink.hpp"
#include "appcontext/get_current_time_as_string.hpp"
#include "components/SampleSummaryComponent/SampleStorage.hpp"
#include "components/SampleSummaryComponent/FlatFileOutputter.hpp"

namespace sample_stats {
	FlatFileOutputter::UniquePtr FlatFileOutputter::create( genfile::CohortIndividualSource const& samples, std::string const& filename, std::string const& analysis_name, Metadata const& metadata ) {
		return UniquePtr( new FlatFileOutputter( samples, filename, analysis_name, metadata ) ) ;
	}

	FlatFileOutputter::SharedPtr FlatFileOutputter::create_shared( genfile::CohortIndividualSource const& samples, std::string const& filename, std::string const& analysis_name, Metadata const& metadata ) {
		return SharedPtr( new FlatFileOutputter( samples, filename, analysis_name, metadata ) ) ;
	}

	FlatFileOutputter::FlatFileOutputter( genfile::CohortIndividualSource const& samples, std::string const& filename, std::string const& analysis_name, Metadata const& metadata ):
		m_filename( filename ),
		m_analysis_name( analysis_name ),
		m_metadata( metadata )
	{
		void(std::vector<genfile::VariantEntry>::*push_back)(genfile::VariantEntry const&) = &std::vector<genfile::VariantEntry>::push_back ;
		samples.get_column_values( "ID_1", boost::bind( push_back, &m_samples, _2 )) ;
	}
	
	FlatFileOutputter::~FlatFileOutputter() {
	}
	
	void FlatFileOutputter::finalise( long ) {
		if( !m_values.empty() ) {
			store_block() ;
			m_values.clear() ;
		}
		m_sink->write_comment( "Completed successfully at " + appcontext::get_current_time_as_string() ) ;
	}

	FlatFileOutputter::AnalysisId FlatFileOutputter::analysis_id() const {
		// A flat file only ever has one analysis.
		return 0 ;
	}

	void FlatFileOutputter::add_variable(
		std::string const& variable
	) {
		VariableMap::left_const_iterator where = m_variables.left.find( variable ) ;
		if( where == m_variables.left.end() ) {
			if( m_sink.get() ) {
				// Uh-oh, have already written a header.
				throw genfile::BadArgumentError( "sample_stats::FlatFileOutputter::add_variable()", "variable=\"" + variable + "\"" ) ;
			}
			else {
				// Still have time to add the variable to our list of variables, retaining the order of addition.
				where = m_variables.left.insert( VariableMap::left_value_type( variable, m_variables.size() ) ).first ;
			}
		}
	}

	void FlatFileOutputter::store_per_sample_data(
		std::string const& computation_name,
		std::size_t sample,
		std::string const& variable,
		std::string const& description,
		genfile::VariantEntry const& value
	) {
		VariableMap::left_const_iterator wVariable = m_variables.left.find( variable ) ;
		if( wVariable == m_variables.left.end() ) {
			if( m_sink.get() ) {
				// Uh-oh, have already written a header.
				throw genfile::BadArgumentError( "sample_stats::FlatFileOutputter::store_per_variant_data()", "variable=\"" + variable + "\"" ) ;
			}
			else {
				// Still have time to add the variable to our list of variables, retaining the order of addition.
				wVariable = m_variables.left.insert( VariableMap::left_value_type( variable, m_variables.size() ) ).first ;
			}
		}

		// Store the value of this variable.
		m_values[ std::make_pair( sample, wVariable->second ) ] = value ;
	}

	void FlatFileOutputter::store_block() {
		if( !m_sink.get() ) {
			m_sink = statfile::BuiltInTypeStatSink::open( m_filename ) ;
			m_sink->write_metadata( format_metadata() ) ;

			(*m_sink) | "sample" | "index" ;
			VariableMap::right_const_iterator
				i = m_variables.right.begin(),
				end_i = m_variables.right.end() ;
			for( ; i != end_i; ++i ) {
				(*m_sink).add_column( i->second ) ;
			}
			(*m_sink) << statfile::begin_data() ;
		}
		for( std::size_t i = 0; i < m_samples.size(); ++i ) {
			genfile::VariantEntry const& sample = m_samples[ i ] ;
			(*m_sink) << sample << uint64_t( i ) ;
			VariableMap::right_const_iterator
				var_i = m_variables.right.begin(),
				end_var_i = m_variables.right.end() ;
			for( ; var_i != end_var_i; ++var_i ) {
				ValueMap::const_iterator where = m_values.find( std::make_pair( i, var_i->first )) ;
				if( where == m_values.end() ) {
					(*m_sink) << genfile::MissingValue() ;
				}
				else {
					(*m_sink) << where->second ;
				}
			}
			(*m_sink) << statfile::end_row() ;
		}
	}
	
	std::string FlatFileOutputter::format_metadata() const {
		std::ostringstream str ;
		str << "Analysis: \"" << m_analysis_name << "\"\n"
			<< " started: " << appcontext::get_current_time_as_string() << "\n" ;
		str << "\nAnalysis properties:\n" ;
		for( Metadata::const_iterator i = m_metadata.begin(); i != m_metadata.end(); ++i ) {
			str << "  "
				<< i->first
				<< " "
				<< genfile::string_utils::join( i->second.first, " " )
				<< " (" + i->second.second + ")\n" ;
		}
		return str.str() ;
	}
}
