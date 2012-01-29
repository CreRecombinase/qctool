#include <iostream>
#include <iomanip>
#include "test_case.hpp"
#include "snptest/case_control/AlternativeModelLogLikelihood.hpp"
#include "Eigen/Eigen"

namespace {
	template< typename M1, typename M2 >
	void check_equal( M1 const& left, M2 const& right, double tolerance = 0.0001 ) {
		BOOST_CHECK_EQUAL( left.rows(), right.rows() ) ;
		BOOST_CHECK_EQUAL( left.cols(), right.cols() ) ;
		for( int i = 0; i < left.rows(); ++i ) {
			for( int j = 0; j < left.cols(); ++j ) {
				BOOST_CHECK_CLOSE( left(i,j), right(i,j), tolerance ) ;
			}
		}
	}
}

void test_alternative_model_certain_genotypes_one_individual( std::size_t g ) {
	std::cerr << "Testing SNPTEST2 alternative model (one individual with certain genotype " << g << ")..." ;
	typedef Eigen::VectorXd Vector ;
	typedef Eigen::MatrixXd Matrix ;

	Vector const phenotypes = Vector::Zero( 1 ) ;
	Matrix genotypes = Matrix::Zero( 1, 3 )  ;
	Vector genotype_levels( 3 ) ;
	genotype_levels << 0, 1, 2 ;
	genotypes( 0, g ) = 1.0 ;

	Vector design_matrix( 2 ) ;
	design_matrix << 1.0, g ;

	Matrix design_matrix_squared( 2, 2 ) ;
	design_matrix_squared <<	1.0, g,
								g, g*g ;

	double p0 ;
	double p1 ;

	snptest::case_control::AlternativeModelLogLikelihood ll ;
	ll.set_phenotypes( phenotypes ) ;
	ll.set_genotypes( genotypes, genotype_levels ) ;

	Vector parameters( 2 ) ;
	// Start at 0,0
	parameters << 0.0, 0.0 ;

	p1 = std::exp( parameters(0) + genotype_levels(g) * parameters(1) ) ;
	p0 = 1.0 / ( 1.0 + p1 ) ;
	p1 = p1 / ( 1.0 + p1 ) ;

	ll.evaluate_at( parameters ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_function(), std::log( p0 ) ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_first_derivative(), -p1 * design_matrix ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_second_derivative(), (( -p1 * ( 1.0 - 2.0 * p1 )) - ( p1 * p1 )) * design_matrix_squared ) ;

	// Nonzero genetic effect parameter
	parameters << 0.0, 1.0 ;
	p1 = std::exp( parameters(0) + genotype_levels(g) * parameters(1) ) ;
	p0 = 1.0 / ( 1.0 + p1 ) ;
	p1 = p1 / ( 1.0 + p1 ) ;
	
	ll.evaluate_at( parameters ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_function(), std::log( p0 ) ) ;
	check_equal( ll.get_value_of_first_derivative(), -p1 * design_matrix, 0.00000000001 ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_second_derivative(), (( -p1 * ( 1.0 - 2.0 * p1 )) - ( p1 * p1 )) * design_matrix_squared ) ;

	// Nonzero baseline parameter instead
	parameters << 1.0, 0.0 ;
	p1 = std::exp( parameters(0) + genotype_levels(g) * parameters(1) ) ;
	p0 = 1.0 / ( 1.0 + p1 ) ;
	p1 = p1 / ( 1.0 + p1 ) ;
	ll.evaluate_at( parameters ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_function(), std::log( p0 ) ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_first_derivative(), -p1 * design_matrix ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_second_derivative(), (( -p1 * ( 1.0 - 2.0 * p1 )) - ( p1 * p1 )) * design_matrix_squared ) ;

	// Both parameters nonzero
	parameters << 1.0, 1.0 ;
	p1 = std::exp( parameters(0) + genotype_levels(g) * parameters(1) ) ;
	p0 = 1.0 / ( 1.0 + p1 ) ;
	p1 = p1 / ( 1.0 + p1 ) ;
	
	ll.evaluate_at( parameters ) ;
	BOOST_CHECK_EQUAL( ll.get_value_of_function(), std::log( p0 ) ) ;
	check_equal( ll.get_value_of_first_derivative(), -p1 * design_matrix, 0.000000000001 ) ;
	Matrix expected_2nd_derivative = (( -p1 * ( 1.0 - 2.0 * p1 )) - ( p1 * p1 )) * design_matrix_squared ;
	BOOST_CHECK_SMALL(( ll.get_value_of_second_derivative() - expected_2nd_derivative ).maxCoeff(), 0.0000000001 ) ;

	std::cerr << "ok.\n" ;
}

AUTO_TEST_CASE( test_alternative_model_one_individual ) {
	test_alternative_model_certain_genotypes_one_individual( 0 ) ;
	test_alternative_model_certain_genotypes_one_individual( 1 ) ;
	test_alternative_model_certain_genotypes_one_individual( 2 ) ;
}

