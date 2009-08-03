
#ifndef __GTOOL__SNPHWE_HPP__
#define __GTOOL__SNPHWE_HPP__

#include "GenotypeAssayStatistics.hpp"

double SNPHWE(int obs_hets, int obs_hom1, int obs_hom2) ;

struct SNPHWEStatistic: public GenotypeAssayStatistic
{
	double calculate_value( GenotypeAssayStatistics const& statistics ) const {
		if( statistics.get_rounded_genotype_amounts().sum() > 0.0 ) {
			return SNPHWE( statistics.get_rounded_genotype_amounts().AB(), statistics.get_rounded_genotype_amounts().AA(), statistics.get_rounded_genotype_amounts().BB() ) ;
		}
		else {
			return 1.0 ;
		}
	}
} ;

#endif
