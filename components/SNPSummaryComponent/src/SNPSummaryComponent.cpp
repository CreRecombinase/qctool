#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH
#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread.hpp>
#include "genfile/SNPIdentifyingData.hpp"
#include "genfile/VariantEntry.hpp"
#include "genfile/vcf/get_set_eigen.hpp"
#include "genfile/SNPDataSourceProcessor.hpp"
#include "components/SNPSummaryComponent/SNPSummaryComponent.hpp"
#include "../../qctool_version_autogenerated.hpp"
#include "components/SNPSummaryComponent/SNPSummaryDBOutputter.hpp"
#include "components/SNPSummaryComponent/FileOutputter.hpp"
#include "components/SNPSummaryComponent/AssociationTest.hpp"
#include "components/SNPSummaryComponent/AncestralAlleleAnnotation.hpp"

void SNPSummaryComputationManager::add_computation( std::string const& name, SNPSummaryComputation::UniquePtr computation ) {
	m_computations.insert( name, computation ) ;
}

void SNPSummaryComputationManager::add_result_callback( ResultCallback callback ) {
	m_result_signal.connect( callback ) ;
}

void SNPSummaryComputationManager::begin_processing_snps( std::size_t number_of_samples ) {
	m_snp_index = 0 ;
}

void SNPSummaryComputationManager::processed_snp( genfile::SNPIdentifyingData const& snp, genfile::VariantDataReader& data_reader ) {
	{
		genfile::vcf::GenotypeSetter< Eigen::MatrixBase< SNPSummaryComputation::Genotypes > > setter( m_genotypes ) ;
		data_reader.get( "genotypes", setter ) ;
	}
	Computations::iterator i = m_computations.begin(), end_i = m_computations.end() ;
	for( ; i != end_i; ++i ) {
		i->second->operator()(
			snp,
			m_genotypes,
			data_reader,
			boost::bind(
				boost::ref( m_result_signal ),
				m_snp_index,
				snp,
				i->first,
				_1,
				_2
			)
		) ;
	}
	++m_snp_index ;
}

void SNPSummaryComputationManager::end_processing_snps() {}

void SNPSummaryComponent::declare_options( appcontext::OptionProcessor& options ) {
	options.declare_group( "SNP computation options" ) ;
	options[ "-snp-stats" ]
		.set_description( "Calculate and output per-SNP statistics.  This implies that no SNP filtering options are used." ) ;
    options[ "-snp-stats-file" ]
        .set_description( 	"Override the auto-generated path(s) of the snp-stats file to use when outputting snp-wise statistics.  "
							"(By default, the paths are formed by adding \".qctool-db\" to the input gen filename(s).)  "
							"The '#' character can also be used here to specify one output file per chromosome." )
        .set_takes_values_per_use(1)
		.set_maximum_multiplicity(1)
		.set_default_value( "" ) ;

	options[ "-snp-stats-columns" ]
        .set_description( "Comma-seperated list of extra columns to output in the snp-wise statistics file.  "
 						"The standard columns are: "
						"SNPID, RSID, position, minor_allele, major_allele, MAF, HWE, missing, information."
						" Your choices here are old_information, jonathans_information, mach_r2, and entropy." )
		.set_takes_single_value()
		.set_default_value( "alleles,HWE,missingness,information" ) ;

	options.declare_group( "Association test options" ) ;
	options[ "-test" ]
		.set_description( "Perform an association test on the given phenotype." )
		.set_takes_single_value() ;
	options[ "-covariates" ]
		.set_description( "Specify a comma-separated list of covariates to use in the association test." )
		.set_takes_single_value()
		.set_default_value( "" ) ;
	
	options[ "-annotate" ]
		.set_description( "Specify a FASTA-formatted file containing ancestral alleles to annotate variants with." )
		.set_takes_single_value() ;
	
	options.option_implies_option( "-snp-stats", "-s" ) ;
	options.option_implies_option( "-annotate", "-s" ) ;
	options.option_implies_option( "-test", "-s" ) ;
}

SNPSummaryComponent::SNPSummaryComponent(
	genfile::CohortIndividualSource const& samples,
	appcontext::OptionProcessor const& options,
	appcontext::UIContext& ui_context
):
	m_samples( samples ),
	m_options( options ),
	m_ui_context( ui_context )
{}

genfile::SNPDataSourceProcessor::Callback::UniquePtr SNPSummaryComponent::create() const {
	genfile::SNPDataSourceProcessor::Callback::UniquePtr result( create_manager().release() ) ;
	return result ;
}

SNPSummaryComputationManager::UniquePtr SNPSummaryComponent::create_manager() const {
	SNPSummaryComputationManager::UniquePtr manager( new SNPSummaryComputationManager() ) ;
	std::string filename = m_options.get_value< std::string >( "-snp-stats-file" ) ;
	
	if( m_options.check( "-nodb" )) {
		if( filename.empty() ) {
			filename = m_options.get< std::string >( "-g" ) + ".qctool.txt" ;
		}
		manager->add_result_callback(
			boost::bind(
				&FileOutputter::operator(),
				FileOutputter::create_shared( filename ),
				_1, _2, _3, _4, _5
			)
		) ;
	}
	else {
		if( filename.empty() ) {
			filename = genfile::strip_gen_file_extension_if_present( m_options.get< std::string >( "-g" ) ) + ".qcdb";
		}
		std::string sample_set_spec = "" ;
		if( m_options.check( "-excl-samples" )) {
			sample_set_spec += "excluded:" + genfile::string_utils::join( m_options.get_values< std::string >( "-excl-samples"  ), "," ) ;
		}
		if( m_options.check( "-incl-samples" )) {
			sample_set_spec += "included:" + genfile::string_utils::join( m_options.get_values< std::string >( "-incl-samples"  ), "," ) ;
		}
	
		impl::SNPSummaryDBOutputter::SharedPtr outputter = impl::SNPSummaryDBOutputter::create_shared(
			filename,
			m_options.get< std::string >( "-analysis-name" ),
			m_options.get< std::string >( "-g" ),
			sample_set_spec
		) ;
	
		manager->add_result_callback(
			boost::bind(
				&impl::SNPSummaryDBOutputter::operator(),
				outputter,
				_1, _2, _3, _4, _5
			)
		) ;
	}
	
	add_computations( *manager ) ;
	return manager ;
}

void SNPSummaryComponent::add_computations( SNPSummaryComputationManager& manager ) const {
	using genfile::string_utils::split_and_strip_discarding_empty_entries ;
	if( m_options.check( "-snp-stats" )) {
		std::vector< std::string > elts = split_and_strip_discarding_empty_entries( m_options.get_value< std::string >( "-snp-stats-columns" ), ",", " \t" ) ;
		foreach( std::string const& elt, elts ) {
			manager.add_computation( elt, SNPSummaryComputation::create( elt )) ;
		}
	}
	if( m_options.check( "-test" )) {
		std::vector< std::string > phenotypes = split_and_strip_discarding_empty_entries( m_options.get_value< std::string >( "-test" ), ",", " \t" ) ;
		std::vector< std::string > covariates ;
		if( m_options.check( "-covariates" ))  {
			covariates = split_and_strip_discarding_empty_entries( m_options.get_value< std::string >( "-covariates" ), ",", " \t" ) ;
		}
		foreach( std::string const& phenotype, phenotypes ) {
			manager.add_computation(
				"association_test",
				AssociationTest::create(
					phenotype,
					covariates,
					m_samples,
					m_options
				)
			) ;
		}
	}
	if( m_options.check( "-annotate" )) {
		appcontext::UIContext::ProgressContext progress = m_ui_context.get_progress_context( "Loading FASTA annotation" ) ;
		manager.add_computation(
			"ancestral_alleles",
			SNPSummaryComputation::UniquePtr(
				new AncestralAlleleAnnotation( m_options.get< std::string >( "-annotate" ), progress )
			)
		) ;
	}
}

SNPSummaryComputation::UniquePtr SNPSummaryComponent::create_computation( std::string const& name ) const {
	if( name != "association_test" ) {
		return SNPSummaryComputation::UniquePtr( SNPSummaryComputation::create( name )) ;
	} else {
		assert(0) ;
	}
}
