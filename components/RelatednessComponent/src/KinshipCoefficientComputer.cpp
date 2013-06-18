
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <boost/ptr_container/ptr_deque.hpp>
#include <boost/function.hpp>
#include <boost/timer/timer.hpp>
#include <boost/thread/thread.hpp>
#include "unistd.h"
#include "../config.hpp"
#if HAVE_CBLAS
	#include "cblas.h"
#endif
#include "Eigen/Core"
#include "Eigen/Eigenvalues"
#include "genfile/SNPIdentifyingData.hpp"
#include "genfile/VariantDataReader.hpp"
#include "genfile/SNPDataSourceProcessor.hpp"
#include "genfile/SingleSNPGenotypeProbabilities.hpp"
#include "genfile/vcf/get_set_eigen.hpp"
#include "statfile/BuiltInTypeStatSink.hpp"
#include "statfile/BuiltInTypeStatSource.hpp"
#include "worker/Task.hpp"
#include "appcontext/FileUtil.hpp"
#include "appcontext/get_current_time_as_string.hpp"
#include "components/RelatednessComponent/KinshipCoefficientComputer.hpp"
#include "components/RelatednessComponent/PCAComputer.hpp"
#include "components/RelatednessComponent/mean_centre_genotypes.hpp"

#include "boost/threadpool.hpp"

#define DEBUG_KINSHIP_COEFFICIENT_COMPUTER 1
#define USING_BOOST_THREADPOOL 1
#define BLOCKWISE_PARALLELISM 1
// #define SNPWISE_PARALLELISM 1

namespace impl {
	#if HAVE_EIGEN
		void accumulate_xxt_using_eigen(
			KinshipCoefficientComputer::Computation::Vector* data,
			KinshipCoefficientComputer::Computation::Matrix* result,
			int const begin_sample_i, int const end_sample_i,
			int const begin_sample_j, int const end_sample_j,
			double const scale
		) {
			if( begin_sample_i == begin_sample_j && end_sample_i == end_sample_j ) {
				result
					->block( begin_sample_i, begin_sample_j, end_sample_i - begin_sample_i, end_sample_j - begin_sample_j )
					.selfadjointView< Eigen::Lower >()
					.rankUpdate( data->segment( begin_sample_i, end_sample_i - begin_sample_i ), scale ) ;
			} else {
				result
					->block(  begin_sample_i, begin_sample_j, end_sample_i - begin_sample_i, end_sample_j - begin_sample_j ).noalias()
					+= scale * (
						data->segment( begin_sample_i, end_sample_i - begin_sample_i )
						*
						data->segment( begin_sample_j, end_sample_j - begin_sample_j ).transpose()
					) ;
			}
		}
	#endif
	#if HAVE_CBLAS
		void accumulate_xxt_using_cblas(
			KinshipCoefficientComputer::Computation::Vector* data,
			KinshipCoefficientComputer::Computation::Matrix* result,
			int const begin_sample_i, int const end_sample_i,
			int const begin_sample_j, int const end_sample_j,
			double const scale
		) {
			assert( result ) ;
			assert( data ) ;
			int const N = data->size() ;
			assert( N == result->cols() ) ;
			if( 0 ) { //begin_sample_i == begin_sample_j && end_sample_i == end_sample_j ) {
				cblas_dsyr(
					CblasColMajor,
					CblasLower,
					end_sample_i - begin_sample_i,
					scale,
					data->segment( begin_sample_i, end_sample_i - begin_sample_i ).data(),
					1,
					result->block( begin_sample_i, begin_sample_j, end_sample_i - begin_sample_i, end_sample_j - begin_sample_j ).data(),
					result->outerStride()
				) ;
			} else {
#if 1
				double x = 0 ;
				for( int i = begin_sample_i; i < end_sample_i; ++i ) {
					for( int j = begin_sample_j; j < end_sample_j; ++j ) {
						//x += i * j ;
						x += ( i << 3 ) * ( j << 3 ) ; 
						// x += (*data)(i) * (*data)(j) ;
						//(*result)(i,j) = x ;
					}
				}
				(*result)(0,0) = x ;
/*				
				cblas_dger(
					CblasColMajor,
					600,
					600,
					scale,
					data->segment( 0, 600 ).data(), 1,
					data->segment( 0, 600 ).data(), 1,
					result->block( 0, 0, 600, 600 ).data(),
					result->outerStride()
				) ;
*/
#else
				cblas_dger(
					CblasColMajor,
					end_sample_i - begin_sample_i,
					end_sample_j - begin_sample_j,
					scale,
					data->segment( begin_sample_i, end_sample_i - begin_sample_i ).data(), 1,
					data->segment( begin_sample_j, end_sample_j - begin_sample_j ).data(), 1,
					result->block( begin_sample_i, begin_sample_j, end_sample_i - begin_sample_i, end_sample_j - begin_sample_j ).data(),
					result->outerStride()
				) ;
#endif
			}
		}
	#endif

	struct SampleBounds {
		int begin_sample_i ;
		int end_sample_i ;
		int begin_sample_j ;
		int end_sample_j ;
	} ;
	
	std::ostream& operator<<( std::ostream& out, SampleBounds const& b ) {
		out << "[" << b.begin_sample_i << "-" << b.end_sample_i << ", " << b.begin_sample_j << "-" << b.end_sample_j << "]" ;
		return out ;
	}
	
#if USING_BOOST_THREADPOOL
	struct ComputeXXtTask { //}: public worker::Task {
#else
struct ComputeXXtTask: public worker::Task {
#endif
		typedef std::auto_ptr< ComputeXXtTask > UniquePtr ;
		
		typedef boost::function< void (
			KinshipCoefficientComputer::Computation::Vector* data,
			KinshipCoefficientComputer::Computation::Matrix* result,
			int const begin_sample_i, int const end_sample_i,
			int const begin_sample_j, int const end_sample_j,
			double const scale
		) >
		AccumulateXXt ;
		
		ComputeXXtTask(
			KinshipCoefficientComputer::Computation::Matrix* result,
			SampleBounds const bounds,
			AccumulateXXt accumulate_xxt,
			std::size_t storage_index
		):
			m_result( result ),
			m_bounds( bounds ),
			m_accumulate_xxt( accumulate_xxt ),
			m_storage_index( storage_index ),
			m_elapsed_time( 0 )
		{
			assert( result ) ;
			assert( m_accumulate_xxt ) ;
		}

		void add_data(
			KinshipCoefficientComputer::Computation::Vector& data
		) {
			m_data.push_back( &data ) ;
		}

		std::size_t number_of_snps() const { return m_data.size() ; }
		std::size_t storage_index() const { return m_storage_index ; }
		double elapsed_cpu_time() const { return m_elapsed_time ; }

		void operator()() {
			boost::timer::cpu_timer timer ;
			for( std::size_t snp_i = 0; snp_i < m_data.size(); ++snp_i ) {
				m_accumulate_xxt(
					m_data[ snp_i ],
					m_result,
					m_bounds.begin_sample_i, m_bounds.end_sample_i,
					m_bounds.begin_sample_j, m_bounds.end_sample_j,
					1.0
				) ;
			}
			m_elapsed_time = timer.elapsed().user + timer.elapsed().system ;
		}
		
	protected:
		KinshipCoefficientComputer::Computation::Matrix* m_result ;
		SampleBounds const m_bounds ;
		std::vector< KinshipCoefficientComputer::Computation::Vector* > m_data ;
		AccumulateXXt m_accumulate_xxt ;
		std::size_t const m_storage_index ;
		double m_elapsed_time ;
	} ;

	struct Dispatcher {
		typedef std::auto_ptr< Dispatcher > UniquePtr ;
		virtual ~Dispatcher() {}
		virtual void setup( std::size_t number_of_samples ) = 0 ;
		virtual void get_storage( KinshipCoefficientComputer::Computation::Vector* data, KinshipCoefficientComputer::Computation::Vector* nonmissingness ) = 0 ;
		virtual void add_data( KinshipCoefficientComputer::Computation::Vector* data, KinshipCoefficientComputer::Computation::Vector* nonmissingness ) = 0 ;
		virtual void wait_until_complete() = 0 ;
	} ;

	struct MultiThreadedDispatcher: public Dispatcher {
		typedef void (*accumulate_xxt_t)( KinshipCoefficientComputer::Computation::Vector*, KinshipCoefficientComputer::Computation::Matrix*, int const begin_sample_i, int const end_sample_i, int const begin_sample_j, int const end_sample_j, double const ) ;
		
		MultiThreadedDispatcher(
			KinshipCoefficientComputer::Computation::Matrix& result,
			KinshipCoefficientComputer::Computation::Matrix& nonmissingness,
			worker::Worker* worker,
			accumulate_xxt_t accumulate_xxt = &accumulate_xxt_using_cblas
		):
			m_result( result ),
			m_nonmissingness( nonmissingness ),
			m_worker( worker ),
			m_storage( 100, KinshipCoefficientComputer::Computation::Vector() ),
			m_storage_index( 0 ),
			m_data_index( 0 ),
			m_accumulate_xxt( accumulate_xxt )
#if USING_BOOST_THREADPOOL
			,m_pool( m_worker->get_number_of_worker_threads() )
#endif
		{}

		~MultiThreadedDispatcher() {
#if DEBUG_KINSHIP_COEFFICIENT_COMPUTER
			std::cerr << "MultiThreadedDispatcher: average times:\n" ;
			for( std::size_t i = 0; i < m_bounds.size(); ++i ) {
				std::cerr << "            (data) " << m_bounds[i] << ": " << std::setprecision(5) << m_task_times[i] / m_task_counts[i] << " over " << m_task_counts[i] << ".\n" ;
			}
			for( std::size_t i = 0; i < m_bounds.size(); ++i ) {
				std::cerr << "  (nonmissingness) " << m_bounds[i] << ": " << std::setprecision(5) << m_task_times[ i + m_bounds.size() ] / m_task_counts[ i + m_bounds.size() ] << " over " << m_task_counts[ i + m_bounds.size() ] << ".\n" ;
			}
#endif
		}
		
		void setup( std::size_t number_of_samples ) {
			// If there are N worker threads we want about two jobs per thread.
			// We imagine splitting up the lower diagonal of the matrix
			// into B jobs, where B is roughly twice the number of threads.
			// Because off-diagonal blocks take about twice as long, 
			// we make them half the size vertically.
			// If there are (N ( N+1)/2 ) square blocks this makes
			// (N (N+1) ) - N = N^2 blocks in total.
			// So we want to choose N so that N^2 is about twice the number of threads.
			std::size_t N = 4 * std::sqrt( m_worker->get_number_of_worker_threads() ) ;
			/*
			if( m_worker->get_number_of_worker_threads() <= 1 ) {
				N = 1 ;
			}
			N = 10 ;
			*/
			N = 5 ;
			std::size_t K = std::ceil( double( number_of_samples ) / N ) ;
			std::size_t Kj = std::ceil( double( number_of_samples ) / ( 2 * N ) ) ;
//#if DEBUG_KINSHIP_COEFFICIENT_COMPUTER
			std::cerr << "MultiThreadedDispatcher: sizeof(int) = " << sizeof(int) << ".\n" ;
			std::cerr << "MultiThreadedDispatcher:setup(): " << number_of_samples << "x" << number_of_samples << " matrix, "
				<< N << " blocks across top row, "
				<< N*(N+1)/2 << " blocks on lower diagonal.\n" ;
//#endif
			// Set up some initial tasks.
			// Do j (the column) as the outer loop
			// so that tasks go in column-major direction.
#if 0
			for( std::size_t j = 0; j < (2*N); ++j ) {
				for( std::size_t i = 0; i < N; ++i ) {
					if( (i*2) >= j ) {
						SampleBounds bounds ;
						bounds.begin_sample_i = (i*K) ;
						bounds.end_sample_i = std::min( (i+1)*K, number_of_samples ) ;
						
						if( i*2 == j ) {
							bounds.begin_sample_j = bounds.begin_sample_i ;
							bounds.end_sample_j = bounds.end_sample_i ;
						}
						else if( (i*2) > j ) {
							bounds.begin_sample_j = (j*Kj) ;
							bounds.end_sample_j = std::min( ( i == 0 && j == 0 ) ? K : (j+1)*Kj, number_of_samples ) ;
						}
						m_bounds.push_back( bounds ) ;
						m_tasks.push_back( 0 ) ;
						m_tasks.push_back( 0 ) ;
//#if DEBUG_KINSHIP_COEFFICIENT_COMPUTER
						std::cerr << "MultiThreadedDispatcher::setup(): added block " << m_bounds.back() << ".\n" ;
//#endif
					}
				}
				if( j == 0 ) {
					++j ;
				}
			}
#else
			for( std::size_t j = 0; j < N; ++j ) {
				for( std::size_t i = 0; i < N; ++i ) {
					if( i >= j ) {
						SampleBounds bounds ;
						bounds.begin_sample_i = (i*K) ;
						bounds.end_sample_i = std::min( (i+1)*K, number_of_samples ) ;

						if( i == j ) {
							bounds.begin_sample_j = bounds.begin_sample_i ;
							bounds.end_sample_j = bounds.end_sample_i ;
						}
						else {
							bounds.begin_sample_j = (j*K) ;
							bounds.end_sample_j = std::min( ( i == 0 && j == 0 ) ? K : (j+1)*K, number_of_samples ) ;
						}
						m_bounds.push_back( bounds ) ;
						m_tasks.push_back( 0 ) ;
						m_tasks.push_back( 0 ) ;
//#if DEBUG_KINSHIP_COEFFICIENT_COMPUTER
						std::cerr << "MultiThreadedDispatcher::setup(): added block " << m_bounds.back() << ".\n" ;
//#endif
					}
				}
			}
#endif
			assert( m_tasks.size() == 2 * m_bounds.size() ) ;
			m_task_times.resize( m_tasks.size(), 0 ) ;
			m_task_counts.resize( m_tasks.size(), 0 ) ;	
		}

		void get_storage( KinshipCoefficientComputer::Computation::Vector* data, KinshipCoefficientComputer::Computation::Vector* nonmissingness ) {
			wait_for_storage( m_storage_index+1 ) ;
			data->swap( m_storage[ m_storage_index % m_storage.size() ] ) ;
			nonmissingness->swap( m_storage[ (m_storage_index+1) % m_storage.size() ] ) ;
		}

		void wait_for_storage( std::size_t storage_index ) {
			// Want to know if there are any tasks still using storage that overlaps with the storage we need.
#if USING_BOOST_THREADPOOL
#else
			for( std::size_t i = 0; i < m_tasks.size(); ++i ) {
				if( !m_tasks.is_null( i ) && m_tasks[i].storage_index() + m_storage.size() <= storage_index ) {
					m_tasks[i].wait_until_complete() ;
				}
			}
#endif
		}

		void wait_until_complete() {
#if USING_BOOST_THREADPOOL
#else
			for( std::size_t i = 0; i < m_tasks.size(); ++i ) {
				m_tasks[i].wait_until_complete() ;
				m_task_times[i] += m_tasks[i].elapsed_cpu_time() / 1E9 ;
				++m_task_counts[i] ;
			}
#endif			
		}

		void add_data( KinshipCoefficientComputer::Computation::Vector* data, KinshipCoefficientComputer::Computation::Vector* nonmissingness ) {
			// Take the data (without copying it).
			data->swap( m_storage[ (m_storage_index) % m_storage.size() ] ) ;
			nonmissingness->swap( m_storage[ (m_storage_index+1) % m_storage.size() ] ) ;
			m_storage_index += 2 ;
	
			bool const schedule = ( m_data_index % 20 ) == 0 ;
			++m_data_index ;

			for( std::size_t task_index = 0; task_index < ( 2 * m_bounds.size() ); ++task_index ) {
				std::size_t const bound_index = task_index % m_bounds.size() ;
				std::size_t const storage_index = ( task_index < m_bounds.size() ) ? m_storage_index - 2 : m_storage_index - 1 ;
				
				if( m_tasks.is_null( task_index ) ) {
					ComputeXXtTask::UniquePtr new_task(
						new ComputeXXtTask(
							( task_index < m_bounds.size() ) ? &m_result: &m_nonmissingness,
							m_bounds[ bound_index ],
							m_accumulate_xxt,
							storage_index
						)
					) ;
					m_tasks.replace( task_index, new_task ) ;
				}
				m_tasks[ task_index ].add_data( m_storage[ storage_index % m_storage.size() ] ) ;

#if USING_BOOST_THREADPOOL
				if( schedule ) {
					m_pool.schedule( boost::bind( &ComputeXXtTask::operator(), &m_tasks[ task_index ] )) ;
				}
#endif
			}
			if( schedule ) {
				m_pool.wait() ;
				for( std::size_t i = 0; i < m_tasks.size(); ++i ) {
					m_tasks.replace( i, 0 ) ;
				}
			}
		}
	
	private:
		KinshipCoefficientComputer::Computation::Matrix& m_result ;
		KinshipCoefficientComputer::Computation::Matrix& m_nonmissingness ;
		worker::Worker* m_worker ;
		std::vector< SampleBounds > m_bounds ;
		boost::ptr_deque< boost::nullable< ComputeXXtTask > > m_tasks ;
		std::vector< KinshipCoefficientComputer::Computation::Vector > m_storage ;
		std::size_t m_storage_index ;
		std::size_t m_data_index ;
		accumulate_xxt_t m_accumulate_xxt ;
		std::vector< double > m_task_times ;
		std::vector< double > m_task_counts ;
		#if USING_BOOST_THREADPOOL
		boost::threadpool::pool m_pool ;
		#endif
	} ;

	NormaliseGenotypesAndComputeXXt::UniquePtr NormaliseGenotypesAndComputeXXt::create( worker::Worker* worker, std::string const& method ) {
		return NormaliseGenotypesAndComputeXXt::UniquePtr( new NormaliseGenotypesAndComputeXXt( worker, method) ) ;
	}

	NormaliseGenotypesAndComputeXXt::NormaliseGenotypesAndComputeXXt(
		worker::Worker* worker,
		std::string const& method
	):
		m_call_threshhold( 0.9 ),
		m_allele_frequency_threshhold( 0.001 ),
		m_number_of_snps_included( 0 ),
		m_dispatcher(
			new impl::MultiThreadedDispatcher(
				m_result,
				m_nonmissingness,
				worker,
#if HAVE_CBLAS
				( method == "eigen" ) ? impl::accumulate_xxt_using_eigen : impl::accumulate_xxt_using_cblas
#else
				accumulate_xxt_using_eigen
#endif
			)
		)
	{}

	KinshipCoefficientComputer::Computation::Matrix const& NormaliseGenotypesAndComputeXXt::result() const { return m_result ; }
	KinshipCoefficientComputer::Computation::Matrix const& NormaliseGenotypesAndComputeXXt::nonmissingness() const { return m_nonmissingness ; }

	void NormaliseGenotypesAndComputeXXt::begin_processing_snps( std::size_t number_of_samples ) {
		m_result.setZero( number_of_samples, number_of_samples ) ;
		m_nonmissingness.setZero( number_of_samples, number_of_samples ) ;
		m_dispatcher->setup( number_of_samples ) ;
	}
		
	void NormaliseGenotypesAndComputeXXt::processed_snp( genfile::SNPIdentifyingData const& id_data, genfile::VariantDataReader::SharedPtr data_reader ) {
		KinshipCoefficientComputer::Computation::Vector genotypes ;
		KinshipCoefficientComputer::Computation::Vector nonmissingness ;

		m_dispatcher->get_storage( &genotypes, &nonmissingness ) ;

		genotypes.setZero( m_result.rows() ) ;
		nonmissingness.setZero( m_result.rows() ) ;

		{
			genfile::vcf::ThreshholdingGenotypeSetter< KinshipCoefficientComputer::Computation::Vector > setter( genotypes, nonmissingness, m_call_threshhold, 0, 0, 1, 2 ) ;
			data_reader->get( "genotypes", setter ) ;
		}

		double allele_frequency = genotypes.sum() / ( 2.0 * nonmissingness.sum() ) ;
		if( std::min( allele_frequency, 1.0 - allele_frequency ) > m_allele_frequency_threshhold ) {
			pca::mean_centre_genotypes( &genotypes, nonmissingness, allele_frequency ) ;
			genotypes /= std::sqrt( ( 2.0 * allele_frequency * ( 1.0 - allele_frequency )) ) ;
			m_dispatcher->add_data( &genotypes, &nonmissingness ) ;
			++m_number_of_snps_included ;
		}
	}
	
	void NormaliseGenotypesAndComputeXXt::end_processing_snps() {
		m_dispatcher->wait_until_complete() ;
		m_result.array() /= m_nonmissingness.array() ;
	}
	
	std::size_t NormaliseGenotypesAndComputeXXt::number_of_snps_included() const {
		return m_number_of_snps_included ;
	}
}

KinshipCoefficientComputer::KinshipCoefficientComputer(
	appcontext::OptionProcessor const& options,
	genfile::CohortIndividualSource const& samples,
	appcontext::UIContext& ui_context,
	Computation::UniquePtr computation
):
	m_options( options ),
	m_ui_context( ui_context ),
	m_samples( samples ),
	m_computation( computation )
{
	assert( m_computation.get() ) ;
}

void KinshipCoefficientComputer::begin_processing_snps( std::size_t number_of_samples ) {
	m_computation->begin_processing_snps( number_of_samples ) ;
}

void KinshipCoefficientComputer::processed_snp( genfile::SNPIdentifyingData const& id_data, genfile::VariantDataReader::SharedPtr data_reader ) {
	m_computation->processed_snp( id_data, data_reader ) ;
}

void KinshipCoefficientComputer::end_processing_snps() {
	m_computation->end_processing_snps() ;

	std::string description = "Number of SNPs: "
		+ genfile::string_utils::to_string( m_computation->number_of_snps_included() )
		+ "\nNumber of samples: "
		+ genfile::string_utils::to_string( m_samples.get_number_of_individuals() )
	;

	send_results(
		m_computation->nonmissingness().cast< double >(),
		m_computation->result().cast< double >(),
		"KinshipCoefficientComputer",
		description
	) ;
}

