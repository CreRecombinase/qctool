
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <Eigen/Core>
#include "test_case.hpp"
#include "metro/likelihood/MultivariateT.hpp"
#include "metro/ValueStabilisesStoppingCondition.hpp"

typedef Eigen::MatrixXd Matrix ;
typedef Eigen::VectorXd Vector ;

double const infinity = std::numeric_limits< double >::infinity() ;

#define DEBUG_MULTIVARIATE_T 1

BOOST_AUTO_TEST_SUITE( test_multivariate_t )

AUTO_TEST_CASE( test_loglikelihood ) {
	{
		Matrix data( 1, 1 ) ;
		data(0,0) = 0 ;

		{
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, infinity ) ;
			T.evaluate_at( Vector::Constant( 1, 0 ), Matrix::Constant( 1, 1, 1 )) ;
			double ll = T.get_value_of_function() ;
			// Tested against mvtnorm's mvt() function in R
			BOOST_CHECK_CLOSE( ll, -0.9189385332046727, 0.000001 ) ;
		}
		{
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 100 ) ;
			T.evaluate_at( Vector::Constant( 1, 0 ), Matrix::Constant( 1, 1, 1 )) ;
			double ll = T.get_value_of_function() ;
			// Tested against mvtnorm's mvt() function in R
			BOOST_CHECK_CLOSE( ll, -0.9214384915429719, 0.000001 ) ;
		}
		{
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 5 ) ;
			T.evaluate_at( Vector::Constant( 1, 0 ), Matrix::Constant( 1, 1, 1 )) ;
			double ll = T.get_value_of_function() ;
			// Tested against mvtnorm's mvt() function in R
			BOOST_CHECK_CLOSE( ll, -0.9686195890547241, 0.000001 ) ;
		}
	}

	{
		Matrix data( 4, 2 ) ;
		data <<
			0.5, 0.1,
			0.4, 0.2,
			0.3, 0.1,
			0.35, 0.15
		;

		{
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, infinity ) ;
			T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 )) ;
			double ll = T.get_value_of_function() ;
			// data = matrix( c( 0.5, 0.1, 0.4, 0.2, 0.3, 0.1, 0.35, 0.15 ), nrow = 4, byrow = T )
			// format( sum( dmvnorm( data, sigma = diag(2), log = TRUE ) ), digits = 16 )
			BOOST_CHECK_CLOSE( ll, -7.704008265637381, 0.000001 ) ;
		}

		{
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 100 ) ;
			T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 )) ;
			double ll = T.get_value_of_function() ;
			// data = matrix( c( 0.5, 0.1, 0.4, 0.2, 0.3, 0.1, 0.35, 0.15 ), nrow = 4, byrow = T )
			// format( sum( dmvt( data, sigma = diag(2), df = 100 ) ), digits = 16 )
			BOOST_CHECK_CLOSE( ll, -7.710705274651815, 0.000001 ) ;
		}

		{
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 5 ) ;
			T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 )) ;
			double ll = T.get_value_of_function() ;
			// data = matrix( c( 0.5, 0.1, 0.4, 0.2, 0.3, 0.1, 0.35, 0.15 ), nrow = 4, byrow = T )
			// format( sum( dmvt( data, sigma = diag(2), df = 5 ) ), digits = 16 )
			BOOST_CHECK_CLOSE( ll, -7.835571956296502, 0.000001 ) ;
		}
	}
}

AUTO_TEST_CASE( test_weighted_loglikelihood ) {

	double ll1 = 0, ll2 = 0, ll3 = 0 ;
	{
		Matrix data( 4, 2 ) ;
		data <<
			0.5, 0.1,
			0.4, 0.2,
			0.3, 0.1,
			0.3, 0.1
		;

		metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 3 ) ;
		T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 )) ;
		ll1 = T.get_value_of_function() ;
	}

	{
		Matrix data( 4, 2 ) ;
		data <<
			0.5, 0.1,
			0.4, 0.2,
			0.3, 0.1,
			0.3, 0.1
		;

		metro::likelihood::MultivariateT< double, Vector, Matrix > T(
			data,
			Vector::Constant( 4, 1 ),
			3
		) ;
		T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 )) ;
		ll2 = T.get_value_of_function() ;
	}

	{
		Matrix data( 3, 2 ) ;
		data <<
			0.5, 0.1,
			0.4, 0.2,
			0.3, 0.1
		;
		Vector weights = Vector::Constant( 3, 1 ) ;
		weights(2) = 2 ;

		metro::likelihood::MultivariateT< double, Vector, Matrix > T(
			data,
			weights,
			3
		) ;
		T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 )) ;
		ll3 = T.get_value_of_function() ;
	}
	
	BOOST_CHECK_CLOSE( ll1, ll2, 0.0000001 ) ;
	BOOST_CHECK_CLOSE( ll1, ll3, 0.0000001 ) ;
}

AUTO_TEST_CASE( test_loglikelihood_range ) {
	{
		double ll1 = 0, ll2 = 0 ;
		metro::ValueStabilisesStoppingCondition stoppingCondition( 0.0000001 ) ;
		{
			Matrix data( 2, 2 ) ;
			data <<
				0.5, 0.1,
				0.4, 0.2
			;

			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 3 ) ;
			T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 )) ;
			ll1 = T.get_value_of_function() ;
			
			stoppingCondition.reset() ;
			T.estimate_by_em( stoppingCondition ) ;
		}

		{
			Matrix data( 5, 2 ) ;
			data <<
				0.5, 0.1,
				0.3, 0.1,
				0.4, 0.2,
				0.45, -100,
				10000, 100000
			;
			metro::DataSubset ranges ;
			ranges.add( metro::DataRange( 0, 1 )) ;
			ranges.add( metro::DataRange( 2, 3 )) ;

			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 3 ) ;
			T.evaluate_at( Vector::Constant( 2, 0 ), Matrix::Identity( 2, 2 ), ranges ) ;
			ll2 = T.get_value_of_function() ;
		}

		// Nominally exactly the same, the computation has slight rounding
		// errors so gets a different value.
		BOOST_CHECK_CLOSE( ll1, ll2, 1E-10 ) ;
	}
}


AUTO_TEST_CASE( test_em ) {

	double const likelihoodTolerance = 0.0000001 ;
	metro::ValueStabilisesStoppingCondition stoppingCondition( likelihoodTolerance ) ;
	double const relativeTolerancePercent = 1E-3 ; // Expect values to differ by no more than 0.001%.
	
	{
#if DEBUG_MULTIVARIATE_T
		std::cerr << "==================================\n" ;
		std::cerr << "test_multivariate_t_em(): 1d test:\n" ;
#endif		
		Matrix data( 3, 1 ) ;
		data <<
			0.25,
			0.75,
			0.2
		;
		
		// mean = 0.5
		// var = 0.125
		// weights should equal 1.
		
		// data = matrix( c( 0.25, 0.75, 0.2 ), ncol = 1, byrow = T )
		// nu = 3
		/* f <- function( params ) {
			mu = params[1] ;
			sigma = matrix( params[2], nrow = 1, ncol = 1 );
			mu = matrix( rep( mu, nrow( data )), nrow = nrow( data ), ncol = ncol( data ), byrow = T )
			D = data - mu
			cat( "-----\n" ) ;
			print( mu );
			print( sigma ) ;
			ll = sum( dmvt( D, sigma = sigma, df = nu, log = T ) )
			return( -ll )
		}
		sigma = var( data )
		mu = colSums( data ) / nrow( data )
		starting.params = c( mu, sigma[1,1] )
		f( starting.params )
		params = optim( starting.params, fn = f, control = list( trace = TRUE ) )
		print( params )
		mu = params$par[1]
		sigma = matrix( params$par[c(2)], nrow = 1 )
		format( mu, digits = 16 )
		format( sigma, digits = 16 )
		format( params$value, digits = 16 )
			*/
		metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 3 ) ;
	
		stoppingCondition.reset() ;
		BOOST_CHECK_EQUAL( T.estimate_by_em( stoppingCondition ), true ) ;
		BOOST_CHECK_CLOSE( T.get_value_of_function(), -0.3457225488574268, relativeTolerancePercent ) ;

#if DEBUG_MULTIVARIATE_T
		std::cerr << "test_multivariate_t_em(): data is:\n"
			<< data << ", estimated parameters are:\n"
			<< "nu = " << T.degrees_of_freedom() << ",\n"
			<< "mean = " << T.mean().transpose() << ",\n"
			<< "sigma =\n" << T.sigma() << ".\n"
			<< "log-likelihood = " << T.get_value_of_function() << ".\n" ;
		
#endif
	}

	{
	// data = matrix( c( 0.5, 0.5, 0.5, 0.6, 0.6, 0.5, 0.6, 0.6, 0.7, 0.54, 0.9, 0.4 ), ncol = 2, byrow = T )
	// nu = 3
	/* f <- function( params ) {
		mu = params[1:2] ;
		sigma = matrix( params[c(3,4,4,5)], nrow = 2, ncol = 2, byrow = T );
		mu = matrix( rep( mu, nrow( data )), nrow = nrow( data ), ncol = ncol( data ), byrow = T )
		D = data - mu
		cat( "-----\n" ) ;
		print( mu );
		print( sigma ) ;
		ll = sum( dmvt( D, sigma = sigma, df = nu, log = T ) )
		return( -ll )
	}
	sigma = var( data )
	mu = colSums( data ) / nrow( data )
	starting.params = c( mu, c( sigma[1,1], sigma[1,2], sigma[2,2] ) )
	f( starting.params )
	params = optim( starting.params, fn = f, control = list( trace = TRUE ) )
	print( params )
	mu = params$par[1]
	sigma = matrix( params$par[c(2)], nrow = 1 )
	format( mu, digits = 16 )
	format( sigma, digits = 16 )
	format( params$value, digits = 16 )
		*/

		Matrix data( 6, 2 ) ;
		data <<
			0.5, 0.5,
			0.5, 0.6,
			0.6, 0.5,
			0.6, 0.6,
			0.7, 0.54,
			0.9, 0.4
		;
		
		{
#if DEBUG_MULTIVARIATE_T
			std::cerr << "==================================\n" ;
#endif
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, infinity ) ;
			stoppingCondition.reset() ;
			T.estimate_by_em( stoppingCondition ) ;

#if DEBUG_MULTIVARIATE_T
			std::cerr << "test_multivariate_t_em(): data is:\n"
				<< data << ", estimated parameters are:\n"
				<< "nu = " << T.degrees_of_freedom() << ",\n"
				<< "mean = " << T.mean().transpose() << ",\n"
				<< "sigma =\n" << T.sigma() << ".\n"
				<< "log-likelihood = " << T.get_value_of_function() << ".\n" ;
#endif
		}

		{
#if DEBUG_MULTIVARIATE_T
			std::cerr << "==================================\n" ;
#endif
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 25 ) ;

			stoppingCondition.reset() ;
			bool converged = T.estimate_by_em( stoppingCondition ) ;
			BOOST_CHECK( converged ) ;

#if DEBUG_MULTIVARIATE_T
			std::cerr << "test_multivariate_t_em(): data is:\n"
				<< data << ", estimated parameters are:\n"
				<< "nu = " << T.degrees_of_freedom() << ",\n"
				<< "mean = " << T.mean().transpose() << ",\n"
				<< "sigma =\n" << T.sigma() << ".\n" ;
#endif
		}

		{
			// data = matrix( c( 0.5, 0.5, 0.5, 0.6, 0.6, 0.5, 0.6, 0.6, 0.7, 0.54, 0.9, 0.4), ncol = 2, byrow = T )
			// nu = 3
			/* f <- function( params ) {
				mu = params[1:2] ;
				sigma = matrix( NA, nrow = 2, ncol = 2 );
				sigma[ lower.tri( sigma, diag = T) ] = params[3:5] ;
				sigma[upper.tri(sigma)] = t(sigma)[ upper.tri(sigma) ] ;
				mu = matrix( rep( mu, nrow( data )), nrow = nrow( data ), ncol = ncol( data ), byrow = T )
				D = data - mu
				print( mu );
				print( D );
				print( sigma ) ;
				ll = sum( dmvt( D, sigma = sigma, df = nu, log = T ) )
				return( ll )
			}
			sigma = var( data )
			mu = colSums( data ) / nrow( data )
			starting.params = c( mu, sigma[1,1], sigma[2,1], sigma[2,2] )
			f( starting.params )
			result = optim( starting.params, fn = f, control = list( fnscale = -1, trace = TRUE, reltol = 1E-16 ) )
			params = result$par
			print( result )
			mu = params[1:2]
			sigma = matrix( params[c(3,4,4,5)], nrow = 2 )
			format( mu, digits = 16 )
			format( sigma, digits = 16 )
			format( result$value, digits = 16 )
			*/

#if DEBUG_MULTIVARIATE_T
			std::cerr << "==================================\n" ;
#endif
			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 3 ) ;
			stoppingCondition.reset() ;
			bool converged = T.estimate_by_em( stoppingCondition ) ;
			BOOST_CHECK_EQUAL( converged, true ) ;
			BOOST_CHECK_CLOSE( T.get_value_of_function(), 12.11434601771716, relativeTolerancePercent ) ;

#if DEBUG_MULTIVARIATE_T
			std::cerr << "test_multivariate_t_em(): data is:\n"
				<< data << ", estimated parameters are:\n"
				<< "nu = " << T.degrees_of_freedom() << ",\n"
				<< "mean = " << T.mean().transpose() << ",\n"
				<< "sigma =\n" << T.sigma() << ".\n"
				<< "log-likelihood= " << T.get_value_of_function() << ".\n" ;
#endif			
			BOOST_CHECK_EQUAL( T.degrees_of_freedom(), 3 ) ;
			BOOST_CHECK_CLOSE( T.mean()(0), 0.6114408499165083, 1 ) ;
			BOOST_CHECK_CLOSE( T.mean()(1), 0.5378066642845329, 1 ) ;
			BOOST_CHECK_CLOSE( T.sigma()(0,0), 0.011982129787457576, 1 ) ;
			BOOST_CHECK_CLOSE( T.sigma()(0,1), -0.003818628929149581, 1 ) ;
			BOOST_CHECK_CLOSE( T.sigma()(1,0), -0.003818628929149581, 1 ) ;
			BOOST_CHECK_CLOSE( T.sigma()(1,1), 0.003392185410464222, 1 ) ;
		}
	}
}

struct MonotonicCheck {
	MonotonicCheck( std::size_t max_iterations = 10000 ):
		m_max_iterations( max_iterations )
	{
		reset() ;
	}
		
	// Return
	bool operator()( double value ) {
		BOOST_CHECK_GE( value, m_value ) ;
		++m_iteration ;
		return( m_iteration > m_max_iterations ) ;
	}

	bool converged() const {
		return false ;
	}

	void reset() {
		m_iteration = 0 ;
		m_value = -std::numeric_limits< double >::infinity() ;
	}

private:
	std::size_t const m_max_iterations ;
	std::size_t m_iteration ;
	double m_value ;
} ;

AUTO_TEST_CASE( test_em_is_monotonic ) {
	MonotonicCheck monotonicCheck( 1000 ) ;
	Matrix data( 16, 2 ) ;
	data <<
		0.5, 0.5,
		0.5, 0.6,
		0.6, 0.5,
		0.6, 0.6,
		0.7, 0.54,
		0.9, 0.4,
		0.1, 0.5,
		0.25, 0.7,
		0.1, 0.1,
		0.15, 0.9,
		0.2, 0.3,
		0.5, 0.55,
		0.4, 0.44,
		0.3, 0.33,
		0.2, 1,
		0.1, 0.15
	;
	
	for( std::size_t nu = 1; nu < 10; ++nu ) {
		monotonicCheck.reset() ;
		metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, nu ) ;
		T.estimate_by_em( monotonicCheck ) ;
	}
}

AUTO_TEST_CASE( test_regularised_em ) {
	double const likelihoodTolerance = 0.0000001 ;
	Matrix data( 1, 2 ) ;
	data <<
		0.5, 0.5
	;
	{
		metro::ValueStabilisesStoppingCondition stoppingCondition( likelihoodTolerance ) ;
		metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 1 ) ;
		bool converged = T.estimate_by_em( stoppingCondition ) ;
		// will not converge because only one observation
		BOOST_CHECK( converged == false ) ;
	}

	{
		metro::ValueStabilisesStoppingCondition stoppingCondition( likelihoodTolerance ) ;
		// try regularised EM.
		metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 1 ) ;
		bool converged = T.estimate_by_em(
			stoppingCondition,
			Matrix::Identity( 2, 2 ),
			1
		) ;
		BOOST_CHECK( converged == true ) ;

		// In effect two observations, one with identity sigma, one with sigma = 0.
		BOOST_CHECK_CLOSE( T.sigma()(0,0), 0.5, 0.01 ) ;
		BOOST_CHECK_CLOSE( T.sigma()(0,1), 0, 0.01 ) ;
		BOOST_CHECK_CLOSE( T.sigma()(1,1), 0.5, 0.01 ) ;
#if DEBUG_MULTIVARIATE_T
			std::cerr << "test_multivariate_t_regularised_em(): data is:\n"
				<< data << ", estimated parameters are:\n"
				<< "nu = " << T.degrees_of_freedom() << ",\n"
				<< "mean = " << T.mean().transpose() << ",\n"
				<< "sigma =\n" << T.sigma() << ".\n"
				<< "log-likelihood= " << T.get_value_of_function() << ".\n" ;
#endif			
	}

	// test a massive weight.
	{
		metro::ValueStabilisesStoppingCondition stoppingCondition( likelihoodTolerance ) ;
		metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, 1 ) ;
		bool converged = T.estimate_by_em(
			stoppingCondition,
			Matrix::Identity( 2, 2 ),
			100000000
		) ;
		BOOST_CHECK( converged == true ) ;
		// Weight is so strong we should get back the prior
		BOOST_CHECK_CLOSE( T.sigma()(0,0), 1, 0.01 ) ;
		BOOST_CHECK_CLOSE( T.sigma()(0,1), 0, 0.01 ) ;
		BOOST_CHECK_CLOSE( T.sigma()(1,1), 1, 0.01 ) ;
	}
}

AUTO_TEST_CASE( test_loglikelihood_em_range ) {
	std::vector< double > nu_values ;
	nu_values.push_back( 1 ) ;
	nu_values.push_back( 3 ) ;
	nu_values.push_back( std::numeric_limits< double >::infinity() ) ;
	
	for( std::size_t i = 0; i < nu_values.size(); ++i ) {
		double nu = nu_values[i] ;
		double ll1 = 0, ll2 = 0 ;
		metro::ValueStabilisesStoppingCondition stoppingCondition( 0.0000001 ) ;
		{
			Matrix data( 3, 2 ) ;
			data <<
				0.5, 0.1,
				0.4, 0.2,
				0.2, 0.3
			;

			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, nu ) ;
			stoppingCondition.reset() ;
			BOOST_CHECK( T.estimate_by_em( stoppingCondition ) ) ;
		
			ll1 = T.get_value_of_function() ;
		}

		{
			Matrix data( 6, 2 ) ;
			data <<
				0.5, 0.1,
				0.3, 0.1,
				0.4, 0.2,
				0.2, 0.3,
				0.45, -100,
				10000, 100000
			
			;
			metro::DataSubset ranges ;
			ranges.add( metro::DataRange( 0, 1 )) ;
			ranges.add( metro::DataRange( 2, 4 )) ;

			metro::likelihood::MultivariateT< double, Vector, Matrix > T( data, nu ) ;
			stoppingCondition.reset() ;
			T.estimate_by_em( ranges, stoppingCondition ) ;
			ll2 = T.get_value_of_function() ;

#if DEBUG_MULTIVARIATE_T
			std::cerr << "test_multivariate_t_em_range ): data is:\n"
				<< data << ", estimated parameters are:\n"
				<< "nu = " << T.degrees_of_freedom() << ",\n"
				<< "mean = " << T.mean().transpose() << ",\n"
				<< "sigma =\n" << T.sigma() << ".\n"
				<< "log-likelihood= " << T.get_value_of_function() << ".\n" ;
#endif
		}

		// Nominally exactly the same, the computation has slight rounding
		// errors so gets a different value.
		BOOST_CHECK_CLOSE( ll1, ll2, 1E-10 ) ;
	}
}

AUTO_TEST_CASE( test_weighted_em ) {
	double const likelihoodTolerance = 0.0000001 ;
	metro::ValueStabilisesStoppingCondition stoppingCondition( 0.0000001 ) ;
	{
		Matrix data1( 4, 2 ) ;
		data1 <<
			0.5, 0.1,
			0.4, 0.2,
			0.3, 0.1,
			0.3, 0.1
		;

		Matrix data2( 3, 2 ) ;
		data2 <<
			0.5, 0.1,
			0.4, 0.2,
			0.3, 0.1
		;
		Vector weights = Vector::Constant( 3, 1 ) ;
		weights(2) = 2 ;

		metro::likelihood::MultivariateT< double, Vector, Matrix > T1( data1, 3 ) ;
		metro::likelihood::MultivariateT< double, Vector, Matrix > T2( data1, Vector::Constant( 4, 1 ), 3 ) ;
		metro::likelihood::MultivariateT< double, Vector, Matrix > T3( data2, weights, 3 ) ;

		T1.estimate_by_em( stoppingCondition ) ;
		stoppingCondition.reset() ;
		T2.estimate_by_em( stoppingCondition ) ;
		stoppingCondition.reset() ;
		T3.estimate_by_em( stoppingCondition ) ;
		stoppingCondition.reset() ;

		BOOST_CHECK_CLOSE( T1.get_value_of_function(), T2.get_value_of_function(), likelihoodTolerance ) ;
		BOOST_CHECK_CLOSE( T1.get_value_of_function(), T3.get_value_of_function(), likelihoodTolerance ) ;
	}
}

BOOST_AUTO_TEST_SUITE_END()
