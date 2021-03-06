
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef QCTOOL_SAMPLE_SUMMARY_COMPONENT_HPP
#define QCTOOL_SAMPLE_SUMMARY_COMPONENT_HPP

#include <memory>
#include "genfile/CohortIndividualSource.hpp"
#include "genfile/SNPDataSourceProcessor.hpp"
#include "appcontext/OptionProcessor.hpp"
#include "appcontext/UIContext.hpp"
#include "components/SampleSummaryComponent/SampleStorage.hpp"

struct SampleSummaryComponent {
public:
	static void declare_options( appcontext::OptionProcessor& options ) ;
	static bool is_needed( appcontext::OptionProcessor const& options ) ;
	typedef std::auto_ptr< SampleSummaryComponent > UniquePtr ;
	static UniquePtr create( appcontext::OptionProcessor const& options, genfile::CohortIndividualSource const& samples, appcontext::UIContext& ui_context ) ;
public:
	SampleSummaryComponent( appcontext::OptionProcessor const& options, genfile::CohortIndividualSource const& samples, appcontext::UIContext& ui_context ) ;
	void setup( genfile::SNPDataSourceProcessor&, sample_stats::SampleStorage::SharedPtr storage ) const ;
private:
	appcontext::OptionProcessor const& m_options ;
	genfile::CohortIndividualSource const& m_samples ;
	appcontext::UIContext& m_ui_context ;
} ;

#endif
