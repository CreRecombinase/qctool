#ifndef QCTOOL_CONCORDANCE_COMPUTATION_HPP
#define QCTOOL_CONCORDANCE_COMPUTATION_HPP

#include <vector>
#include "genfile/SNPIdentifyingData.hpp"
#include "genfile/SingleSNPGenotypeProbabilities.hpp"
#include "appcontext/OptionProcessor.hpp"
#include "appcontext/UIContext.hpp"
#include "SampleBySampleComputation.hpp"

struct ConcordanceComputation: public SampleBySampleComputation {
	ConcordanceComputation( appcontext::OptionProcessor const& options, appcontext::UIContext& ui_context ) ;
	
	void prepare(
		std::vector< genfile::SNPIdentifyingData > const& snps,
		std::vector< genfile::SingleSNPGenotypeProbabilities > const& genotypes
	) {}
	
	// Return the number of genotypes that are called and equal between the two samples.
	double operator()(
		std::size_t const sample1,
		std::size_t const sample2,
		std::vector< genfile::SingleSNPGenotypeProbabilities > const& genotypes
	) ;	
	
private:
	double m_threshhold ;
} ;

struct PairwiseNonMissingnessComputation: public SampleBySampleComputation {
	PairwiseNonMissingnessComputation( appcontext::OptionProcessor const& options, appcontext::UIContext& ui_context ) ;
	
	void prepare(
		std::vector< genfile::SNPIdentifyingData > const& snps,
		std::vector< genfile::SingleSNPGenotypeProbabilities > const& genotypes
	) {}
	
	// Return the number of genotypes that are not missing in either sample.
	double operator()(
		std::size_t const sample1,
		std::size_t const sample2,
		std::vector< genfile::SingleSNPGenotypeProbabilities > const& genotypes
	) ;	

private:
	double m_threshhold ;
} ;

#endif