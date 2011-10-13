#include <boost/ptr_container/ptr_vector.hpp>
#include "genfile/SNPDataSourceProcessor.hpp"
#include "genfile/SNPIdentifyingData.hpp"
#include "genfile/VariantEntry.hpp"
#include "genfile/vcf/get_set.hpp"
#include "genfile/string_utils/substitute.hpp"
#include "worker/Worker.hpp"
#include "Eigen/Core"
#include "ClusterPlotter.hpp"
#include "../config.hpp"
#if HAVE_MGL
	#include "mgl/mgl.h"
	#include "mgl/mgl_zb.h"
#endif

void ClusterPlotter::declare_options( appcontext::OptionProcessor& options ) {
	options.declare_group( "Cluster plot options" ) ;
#if HAVE_MGL
	options[ "-plot-clusters" ]
		.set_description( "Create a plot of the intensities and genotypes for each SNP. "
		 	"Currently this uses the MathGL library (http://mathgl.sourceforge.net) and so only works "
			"if this is detected during compilation.")
		.set_takes_single_value() ;
	options[ "-cluster-plot-filename" ]
		.set_description( "Filename of cluster plots to create" )
		.set_takes_single_value()
		.set_default_value( "${snpid}_${rsid}_${chromosome}_${position}_${callset}_${intensities}.png" ) ;
#endif
}

ClusterPlotter::UniquePtr ClusterPlotter::create( appcontext::OptionProcessor const& options, worker::Worker* worker ) {
	UniquePtr result(
		new ClusterPlotter(
			options.get< std::string >( "-cluster-plot-filename" ),
			genfile::string_utils::split_and_strip_discarding_empty_entries( options.get< std::string >( "-plot-clusters" ), ",", " \t" ),
			worker
		)
	) ;
	return result ;
}

ClusterPlotter::ClusterPlotter(
	std::string const& filename_template,
	std::vector< std::string > const& call_fields,
	worker::Worker* worker
):
	m_filename_template( filename_template ),
	m_call_fields( call_fields ),
	m_intensity_field( "XY" ),
	m_worker( worker ),
	m_max_tasks( 10 )
{}

void ClusterPlotter::begin_processing_snps( std::size_t number_of_samples, std::size_t number_of_snps ) {
	m_number_of_samples = number_of_samples ;
}

#if HAVE_MGL

namespace impl {
	struct PlotTask: public worker::Task {
		typedef Eigen::Matrix< double, 2, Eigen::Dynamic, Eigen::RowMajor > IntensityMatrix ;
		typedef std::vector< int > Genotypes ;
		typedef std::map< std::string, Genotypes > Calls ;
		PlotTask(
			std::string const& cohort_name,
			std::string const& intensity_field,
			std::vector< std::string > const& call_fields,
			genfile::SNPIdentifyingData const& snp,
			genfile::VariantDataReader& data_reader,
			std::string const& filename
		):
			m_cohort_name( cohort_name ),
			m_intensity_field( intensity_field ),
			m_snp( snp ),
			m_filename( filename ),
			m_call_threshhold( 0.9 )
		{
			for( std::size_t i = 0; i < call_fields.size(); ++i ) {
				m_calls[ call_fields[i] ] = Genotypes() ;
			}
			genfile::vcf::MatrixSetter< IntensityMatrix > intensity_setter( m_intensities ) ;

			data_reader.get( m_intensity_field, intensity_setter ) ;
			for( Calls::iterator i = m_calls.begin(), end_i = m_calls.end(); i != end_i; ++i ) {
				genfile::vcf::GenotypeSetter< Genotypes > genotype_setter( i->second, m_call_threshhold, 3, 0, 1, 2 ) ;
				data_reader.get( i->first, genotype_setter ) ;
				assert( i->second.size() == m_intensities.cols() ) ;
			}
		}
		
		void operator()() {
			int M = std::ceil( std::sqrt( m_calls.size() ) ) ;
			int N = std::ceil( m_calls.size() / M ) ;
			mglGraphZB graph( 400 * N, 400 * M ) ;
			mglData x( m_intensities.cols() ), y( m_intensities.cols() ), colour( m_intensities.cols() ) ;
			
			std::size_t count = 0 ;
			for( Calls::iterator i = m_calls.begin(), end_i = m_calls.end(); i != end_i; ++i, ++count ) {
				x.Set( m_intensities.row(0).data(), m_intensities.cols() ) ;
				y.Set( m_intensities.row(1).data(), m_intensities.cols() ) ;
				colour.Set( i->second ) ;

				graph.Title( m_snp.get_rsid().c_str(), 0, 4 ) ;
				graph.SubPlot( N, M, count ) ;
				graph.SetTickLen( 0.04 ) ;
				graph.SetRanges( 0.0, 5.0, 0.0, 5.0 ) ;
				graph.SetCut( false ) ;
				graph.Axis( "x", true ) ;
				graph.Axis( "y", true ) ;
				graph.CAxis( 0.0, 3.0 ) ;

				graph.Tens( x, y, colour, "RGBk ." ) ;
				graph.Puts( mglPoint( 2.5, 3.0 ), ( i->first + "/" + m_intensity_field ).c_str() ) ;
			}
			graph.WritePNG( m_filename.c_str() ) ;
		}
	private:
		Calls m_calls ;
		std::string const m_intensity_field ;
		IntensityMatrix m_intensities ;
		double const m_call_threshhold ;
		genfile::SNPIdentifyingData const m_snp ;
		
		std::string const m_cohort_name ;
		std::string const m_call_field ;
		std::string const m_filename ;
	} ;
}

void ClusterPlotter::processed_snp( genfile::SNPIdentifyingData const& snp, genfile::VariantDataReader& data_reader ) {
	std::auto_ptr< impl::PlotTask > plot_task(
		new impl::PlotTask(
			"cohort1",
			m_intensity_field,
			m_call_fields,
			snp,
			data_reader,
			snp.get_rsid() + "_" + m_intensity_field + ".png"
		)
	) ;
		
	if( m_tasks.size() < m_max_tasks ) {
		m_tasks.push_back( plot_task ) ;
		m_worker->tell_to_perform_task( m_tasks.back() ) ;
	} else {
		// wait until a task is free...
		bool free = false ;
		std::size_t i = m_tasks.size() ;
		while( i == m_tasks.size() ) {
			for( i = 0; i < m_tasks.size(); ++i ) {
				if( m_tasks[i].check_if_complete() ) {
					break ;
				}
				boost::this_thread::sleep( boost::posix_time::milliseconds( 10 ) ) ;
			}
		}
		assert( i != m_tasks.size() ) ;
		m_tasks.replace( i, plot_task ) ;
		m_worker->tell_to_perform_task( m_tasks[i] ) ;
	}
}
#else
void ClusterPlotter::processed_snp( genfile::SNPIdentifyingData const& snp, genfile::VariantDataReader& data_reader ) {
	assert(0) ;
}
#endif

void ClusterPlotter::end_processing_snps() {
	// nowt to do.
}
