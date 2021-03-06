
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef QCTOOL_SAMPLE_SUMMARY_COMPUTATION_HPP
#define QCTOOL_SAMPLE_SUMMARY_COMPUTATION_HPP

#include <memory>
#include <boost/function.hpp>
#include <Eigen/Core>
#include "genfile/CohortIndividualSource.hpp"
#include "genfile/VariantDataReader.hpp"
#include "genfile/VariantIdentifyingData.hpp"
#include "genfile/VariantEntry.hpp"
#include "appcontext/OptionProcessor.hpp"

struct SampleSummaryComputation: public boost::noncopyable {
	typedef std::auto_ptr< SampleSummaryComputation > UniquePtr ;
	typedef Eigen::MatrixXd Genotypes ;
	typedef boost::function< void ( std::size_t sample_i, std::string const& value_name, genfile::VariantEntry const& value ) > ResultCallback ;
	virtual ~SampleSummaryComputation() {}
	virtual void accumulate( genfile::VariantIdentifyingData const&, Genotypes const&, genfile::VariantDataReader& ) = 0 ;
	virtual void compute( int sample, ResultCallback ) = 0 ;
	virtual std::string get_summary( std::string const& prefix = "", std::size_t column_width = 20 ) const = 0 ;
} ;

#endif
