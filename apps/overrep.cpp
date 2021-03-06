
//          Copyright Gavin Band 2008 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <memory>
#include <set>
#include <map>
#include <utility>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH
#include <boost/regex.hpp>
#include <boost/math/distributions/hypergeometric.hpp>
#include "appcontext/CmdLineOptionProcessor.hpp"
#include "appcontext/ApplicationContext.hpp"
#include "appcontext/ProgramFlow.hpp"
#include "appcontext/get_current_time_as_string.hpp"
#include "genfile/FileUtils.hpp"
#include "genfile/utility.hpp"
#include "statfile/BuiltInTypeStatSource.hpp"
#include <Eigen/Core>

namespace globals {
	std::string const program_name = "overrep" ;
	std::string const program_version = "0.1" ;
}

struct OverrepOptions: public appcontext::CmdLineOptionProcessor {
	std::string get_program_name() const { return globals::program_name ; }

	void declare_options( appcontext::OptionProcessor& options ) {
		options.set_help_option( "-help" ) ;

		options.declare_group( "File handling options" ) ;
		options[ "-p" ]
			.set_description( "Specify the path of a file containing pathway definitions to load."
			 	" This file must have at least three columns; the first should be an identifier for the pathway (no whitespace),"
				" the second should be the pathway name, and the third should contain gene identifiers." )
			.set_takes_values_until_next_option()
			.set_minimum_multiplicity( 1 )
			.set_maximum_multiplicity( 100 )
		;

		options[ "-c" ]
			.set_description( "Specify the path of a file containing gene clusters to load." )
			.set_takes_single_value()
		;

		options[ "-u" ]
			.set_description( "Specify the path of a file containing a list of genes to treat as the gene \"universe\"." )
			.set_takes_single_value()
			.set_is_required()
		;

		options[ "-g" ]
			.set_description( "Specify the path of a file containing a list of genes to test with." )
			.set_takes_single_value()
		;

		options.declare_group( "Miscellaneous options" ) ;
		options[ "-P-value" ]
			.set_description( "Output lists of genes for all pathways getting this P-value or better." )
			.set_takes_single_value()
			.set_default_value( 0.000001 )
		;
		options[ "-analysis-name" ]
			.set_description( "Specify a name to label results from this analysis with.  (This applies to modules which store their results in a qcdb file.)" )
			.set_takes_single_value()
			.set_is_required() ;

		options[ "-intersect" ]
			.set_description( "Specify that the universe should only include genes that actuall occur in pathways." ) ;

		options[ "-log" ]
			.set_description( "Specify the path of the log file." )
			.set_takes_single_value()
			.set_default_value( "overrep.log" ) ;
	}
} ;

// Fisher's exact test between categories
//               red           black
//     good |     a      |       b        |
//      bad |     c      |       d        |
//
// Under the null that both rows are distributed the same, a is hypergeometrically
// distributed.  Fishers' p-value is then the mass under the hypergeometric distribution
// of all values of a greater than or equal to the one given that are consistent with
// the given margins of the table.
struct FishersExactTest {
	FishersExactTest( Eigen::Matrix2d const& matrix ):
		m_matrix( matrix ),
		m_distribution( m_matrix.col( 0 ).sum(), m_matrix.row( 0 ).sum(), m_matrix.sum() )
	{}
	
	double get_OR() const {
		return ( m_matrix(0,0) * m_matrix(1,1) ) / ( m_matrix(0,1)*m_matrix(1,0) ) ;
	}

	std::pair< double, double > get_confidence_interval() const {
		assert(0) ;
	}

	double get_pvalue() const {
		using boost::math::cdf ;
		using boost::math::complement ;

		if( m_matrix(0,0) == 0 ) {
			return 1 ;
		} else {
			return cdf( complement( m_distribution, m_matrix( 0, 0 ) - 1.0 )) ;
		}
	}

	private:
		Eigen::Matrix2d const m_matrix ;
		boost::math::hypergeometric m_distribution ;
} ;

void load_list_of_strings( std::string const& filename, boost::function< void ( std::string const& ) > setter ) {
	std::auto_ptr< std::istream > f = genfile::open_text_file_for_input( filename ) ;
	std::string element ;
	while( (*f) >> element ) {
		setter( element ) ;
	}
}

void load_pathways_list(
	statfile::BuiltInTypeStatSource& source,
	boost::function< void ( std::string const&, std::string const& ) > set_pathway_name,
	boost::function< void ( std::string const&, std::string const& ) > set_pathway_gene
) {
	std::string pathway_id ;
	std::string pathway_name ;
	std::string gene_id ;
	while( source >> pathway_id ) {
		source >> pathway_name >> gene_id >> statfile::ignore_all() ;
		set_pathway_name( pathway_id, pathway_name ) ;
		set_pathway_gene( pathway_id, gene_id ) ;
	}
}

void load_clusters(
	statfile::BuiltInTypeStatSource& source,
	boost::function< void ( std::string const&, std::string const& ) > set_cluster_member = boost::function< void ( std::string const&, std::string const& ) >(),
	boost::function< void ( std::string const&, std::string const& ) > set_cluster_comment = boost::function< void ( std::string const&, std::string const& ) >()
) {
	std::string cluster_id ;
	std::string cluster_comment ;
	std::string gene_regexp ;
	while( source >> cluster_id ) {
		source >> gene_regexp >> cluster_comment >> statfile::ignore_all() ;
		if( set_cluster_comment ) {
			set_cluster_comment( cluster_id, cluster_comment ) ;
		}
		if( set_cluster_member ) {
			set_cluster_member( cluster_id, gene_regexp ) ;
		}
	}
}

namespace impl {
	struct MapSetter {
		typedef std::string A ;
		typedef std::string B ;
		typedef std::map< A, B > Map ;
		MapSetter( Map& map ): m_map( &map ) {} ;
		MapSetter( MapSetter const& other ): m_map( other.m_map ) {} ;
		MapSetter& operator=( MapSetter const& other ) {
			m_map = other.m_map ;
			return *this ;
		} 
		
		void operator()( std::string const& left, std::string const& right ) const {
			(*m_map)[ left ] = right ;
		}
		
	private:
		Map* m_map ;
	} ;
	
	struct MapSetSetter {
		typedef std::string A ;
		typedef std::set< std::string > B ;
		typedef std::map< A, B > Map ;
		MapSetSetter( Map& map ): m_map( &map ) {} ;
		MapSetSetter( MapSetSetter const& other ): m_map( other.m_map ) {} ;
		MapSetSetter& operator=( MapSetSetter const& other ) {
			m_map = other.m_map ;
			return *this ;
		} 
		
		void operator()( std::string const& left, std::string const& right ) const {
			(*m_map)[ left ].insert( right ) ;
		}
		
	private:
		mutable Map* m_map ;
	} ;
	
	template< typename Container >
	void insert_into( typename Container::value_type const& value, Container* X ) {
		X->insert( value ) ;
	}
	
	template< typename Container >
	bool in( typename Container::value_type const& value, Container const& X ) {
		return X.find( value ) != X.end() ;
	}
}

struct OverrepApplication: public appcontext::ApplicationContext {
public:
	OverrepApplication( int argc, char **argv ):
		appcontext::ApplicationContext(
			globals::program_name,
			globals::program_version,
			std::auto_ptr< appcontext::OptionProcessor >( new OverrepOptions ),
			argc,
			argv,
			"-log"
		)
	{}
	
	void process() {
		unsafe_process() ;
	}
	
private:
	typedef std::set< std::string > StringSet ;
	typedef std::map< std::string, std::string > StringStringMap ;
	typedef std::map< std::string, std::set< std::string > > StringStringSetMap ;

	void unsafe_process() {
		load_data() ;
		m_cluster_mapping = get_cluster_mapping() ;
		map_genes_through_clusters( m_cluster_mapping ) ;

		summarise() ;
		
		run_tests() ;
	}

	void load_data() {
		StringStringMap pathway_names ;
		StringStringSetMap pathway_members ;
		StringStringSetMap clusters ;
		StringStringMap cluster_descriptions ;
		StringSet universe ;
		StringSet test_genes ;
		
	
		assert( options().check( "-u" ) ) ;	
		if( options().check( "-u" )) {
			load_list_of_strings(
				options().get< std::string >( "-u" ),
				boost::bind(
					&impl::insert_into< StringSet >,
					_1,
					&universe
				)
			) ;

			get_ui_context().logger()
				<< "I loaded " << universe.size() << " universe genes.  First few are:\n" ;
			StringSet::const_iterator
				i = universe.begin(),
				end_i = universe.end() ;
			for( std::size_t count = 0; count < 5 && i != end_i; ++count, ++i ) {
				get_ui_context().logger() << "  " << *i << "\n" ;
			}
		}
		
		if( options().check( "-c" )) {
			statfile::BuiltInTypeStatSource::UniquePtr source = statfile::BuiltInTypeStatSource::open(
				options().get< std::string >( "-c" )
			) ;
			load_clusters(
				*source,
				impl::MapSetSetter( clusters ),
				impl::MapSetter( cluster_descriptions )
			) ;
			get_ui_context().logger() << "Loaded " << clusters.size() << " clusters:\n" ;
			foreach( StringStringSetMap::value_type const& elt, clusters ) {
				get_ui_context().logger()
					<< "  " << elt.first << ": " ;
				foreach( StringSet::value_type const& value, elt.second ) {
					get_ui_context().logger() << value << " " ;
				}
				get_ui_context().logger() << "\n" ;
			}
		}

		if( options().check( "-p" )) {
			std::vector< std::string > elts = options().get_values< std::string >( "-p" ) ;
			foreach( std::string const& elt, elts ) {
				statfile::BuiltInTypeStatSource::UniquePtr source = statfile::BuiltInTypeStatSource::open(
					genfile::wildcard::find_files_by_chromosome( elt )
				) ;
				load_pathways_list(
					*source,
					impl::MapSetter( pathway_names ),
					impl::MapSetSetter( pathway_members )
				) ;
			}

			typedef std::pair< std::string, std::set< std::string > > StringStringSet ;
			if( options().check( "-intersect" )) {
				StringSet pathway_universe ;
				foreach( StringStringSet const& entry, pathway_members ) {
					pathway_universe.insert( entry.second.begin(), entry.second.end() ) ;
				}
				get_ui_context().logger() << "Created pathway universe of " << universe.size() << " genes based on all genes in pathways.\n" ;
				std::size_t const N = universe.size() ;
				universe = genfile::utility::intersect( universe, pathway_universe ) ;
				get_ui_context().logger() << "Formed intersection with universe (" << universe.size() << " of " << N << " in original universe.\n" ;
				
			}

			std::map< std::string, StringSet > universe_pathway_members ;
			foreach( StringStringSet const& entry, pathway_members ) {
				universe_pathway_members[ entry.first ] = genfile::utility::intersect( entry.second, universe ) ;
			}
			
			get_ui_context().logger()
				<< "I loaded " << pathway_names.size() << " pathways.  First few are:\n" ;
			std::map< std::string, std::string >::const_iterator
				pathway_i = pathway_names.begin(),
				pathway_end = pathway_names.end() ;
			for( std::size_t count = 0; count < 5 && pathway_i != pathway_end; ++count, ++pathway_i ) {
				get_ui_context().logger() << "  " << pathway_i->first << ": " << pathway_i->second
					<< " (" << pathway_members[pathway_i->first].size()
					<< " genes of which "
					<< universe_pathway_members[pathway_i->first].size()
					<<" are in the universe)\n" ;
			}
			
			pathway_members = universe_pathway_members ;
		}
		
		if( options().check( "-g" )) {
			load_list_of_strings(
				options().get< std::string >( "-g" ),
				boost::bind(
					impl::insert_into< StringSet >,
					_1,
					&test_genes
				)
			) ;
			
			get_ui_context().logger()
				<< "I loaded " << test_genes.size() << " test genes " ;
			test_genes = genfile::utility::intersect( test_genes, universe ) ;
			get_ui_context().logger()
				<< "of which " << test_genes.size() << " are in the universe.  First few are:\n" ;
			StringSet::const_iterator
				i = test_genes.begin(),
				end_i = test_genes.end() ;
			for( std::size_t count = 0; count < 5 && i != end_i; ++count, ++i ) {
				get_ui_context().logger() << "  " << *i << "\n" ;
			}
		}
		
		m_pathway_names = pathway_names ;
		m_pathway_members = pathway_members ;
		m_clusters = clusters ;
		m_cluster_descriptions = cluster_descriptions ;
		m_universe = universe ;
		m_test_genes = test_genes ;
	}
	
	StringStringMap get_cluster_mapping() {
		get_ui_context().logger() << "Computing gene->cluster mapping...\n" ;
		StringStringMap result ;
		foreach( StringStringSetMap::value_type const& cluster, m_clusters ) {
			foreach( std::string const& regex_string, cluster.second ) {
				boost::regex regex( regex_string ) ;
				StringStringMap const& this_map = get_cluster_mapping( regex, cluster.first ) ;
				result.insert( this_map.begin(), this_map.end() ) ;
			}
		}
		
		return result ;
	}
	
	StringStringMap get_cluster_mapping( boost::regex const& regex, std::string const& replacement ) {
		StringStringMap result ;
		get_cluster_mapping( m_universe, regex, replacement, boost::bind( impl::insert_into< StringStringMap >, _1, &result )) ;
		get_cluster_mapping( m_test_genes, regex, replacement, boost::bind( impl::insert_into< StringStringMap >, _1, &result )) ;
		foreach( StringStringSetMap::value_type& value, m_pathway_members ) {
			get_cluster_mapping( value.second, regex, replacement, boost::bind( impl::insert_into< StringStringMap >, _1, &result )) ;
		}
		return result ;
	}

	
	void get_cluster_mapping(
		StringSet& values,
		boost::regex const& regex,
		std::string const& replacement,
		boost::function< void ( std::pair< std::string, std::string > const& ) > output
	) {
		StringSet::iterator i = values.begin(), end_i = values.end() ;
		for( ; i != end_i; ++i ) {
			std::string value = *i ;
			value = boost::regex_replace( value, regex, replacement ) ;
			if( value != *i ) {
				output( std::make_pair( *i, value ) ) ;
			}
		}
	}

	void map_genes_through_clusters( StringStringMap const& mapping ) {
		get_ui_context().logger() << "Mapping genes through clusters...\n" ;
		std::size_t n = m_universe.size() ;
		m_universe = map_genes_through_clusters( mapping, m_universe ) ;
		get_ui_context().logger() << "Mapped " << n << " genes to " << m_universe.size() << " in universe.\n" ;
		n = m_test_genes.size() ;
		m_test_genes = map_genes_through_clusters( mapping, m_test_genes ) ;
		get_ui_context().logger() << "Mapped " << n << " genes to " << m_test_genes.size() << " in test genes.\n" ;
		foreach( StringStringSetMap::value_type& value, m_pathway_members ) {
			value.second = map_genes_through_clusters( mapping, value.second ) ;
		}
	}

	StringSet map_genes_through_clusters( StringStringMap const& mapping, StringSet const& genes ) {
		StringSet result ;
		foreach( StringSet::value_type const& value, genes ) {
			StringStringMap::const_iterator where = mapping.find( value ) ;
			if( where != mapping.end() ) {
				result.insert( where->second ) ;
			} else {
				result.insert( value ) ;
			}
        }
		return result ;
	}

	void summarise() {
		get_ui_context().logger() << "\n-------------------------\n\n" ;
		if( m_clusters.size() > 0 ) {
			get_ui_context().logger() << std::setw( 12 ) << "Cluster mapping:\n" ;
            StringStringSetMap backMap ;
			foreach( StringStringMap::value_type const& key_value, m_cluster_mapping ) {
                backMap[ key_value.second ].insert( key_value.first ) ;
			}
            foreach( StringStringSetMap::value_type const& k, backMap ) {
                get_ui_context().logger() << std::setw( 12 ) << k.first << ":" ;
				std::size_t count = 0 ;
				for( StringSet::const_iterator i = k.second.begin(); i != k.second.end(); ++i, ++count ) {
                    get_ui_context().logger() << " " << *i ;
					if( count == 10 ) {
						get_ui_context().logger() << " (+ " << k.second.size() - 10 << " others)..." ;
						break ;
					}
                }
                get_ui_context().logger() << "\n" ;
            }
		}
#if 0
		if( m_clusters.size() > 0 ) {
			get_ui_context().logger() << std::setw( 12 ) << "Cluster mapping:\n" ;
			foreach( StringStringMap::value_type const& key_value, m_cluster_mapping ) {
				get_ui_context().logger() << std::setw( 12 ) << key_value.first << ": "  << key_value.second << "\n" ;
			}
		}
#endif
		get_ui_context().logger() << std::setw( 12 ) << "Test genes:" << "  " << m_test_genes.size() << " genes\n" ;
		get_ui_context().logger() << std::setw( 12 ) << "Pathways:" << "  " << m_pathway_names.size() << " pathways\n" ;
		get_ui_context().logger() << std::setw( 12 ) << "Universe:" << "  " << m_universe.size() << " genes\n" ;
	}

	void run_tests() {
		// table is
		//                 in pathway | not in pathway |  
		//     test gene |     a      |       b        |
		// Not test gene |     c      |       d        |
		//
		Eigen::Matrix2d table ;
		
		using std::setw ;
		std::string const tab = "\t" ;
		std::cout
		//get_ui_context().logger()
			<< "analysis" << tab
			<< "pathway id" << tab
			<< "pathway name" << tab
			<< "hits in pathway" << tab
			<< "hits not in pathway" << tab
			<< "nonhits in pathway" << tab
			<< "nonhits not in pathway" << tab
			<< "total in pathway" << tab
			<< "total genes" << tab
			<< "sample odds ratio" << tab
			<< "fishers exact test p value" << tab
			<< "ids.of.hits.in.pathway"
			<< "\n" ;

		std::string const analysis = options().get< std::string >( "-analysis-name" ) ;
		double const pvalue_threshhold = options().get< double >( "-P-value" ) ;
		std::ostringstream hit_genes_in_pathway ;
		foreach( StringStringSetMap::value_type const& pathway, m_pathway_members ) {
			hit_genes_in_pathway.str( "" ) ;
			table = Eigen::Matrix2d::Zero() ;

			foreach( std::string const& value, m_universe ) {
				int i = 0, j = 0 ;
				if( impl::in( value, m_test_genes )) {
					i = 0 ;
				} else {
					i = 1 ;
				}

				if( impl::in( value, pathway.second ) ) {
					j = 0 ;
				} else {
					j = 1 ;
				}
				
				++table( i, j ) ;

				if( i == 0 && j == 0 ) {
					if( hit_genes_in_pathway.str().size() == 0 ) {
						hit_genes_in_pathway << value ;
					}
					else {
						hit_genes_in_pathway << "," << value ;
					}
				}
			}

			//get_ui_context().logger()
			std::cout
				<< analysis << tab	
				<< pathway.first << tab
				<< "\"" << m_pathway_names[ pathway.first ] << "\"" << tab
				<< table( 0, 0 ) << tab
				<< table( 0, 1 ) << tab
				<< table( 1, 0 ) << tab
				<< table( 1, 1 ) << tab
				<< table.col( 0 ).sum() << tab
				<< table.sum() << tab ;
			try {
				FishersExactTest test( table ) ;
				//get_ui_context().logger()
				std::cout
					<< test.get_OR() << tab << test.get_pvalue() ;
				if( test.get_pvalue() <= pvalue_threshhold ) {
					std::cout << tab << hit_genes_in_pathway.str() ;
				} else {
					std::cout << tab << "NA" ;
				}
			}
			catch( std::exception const& ) {
				// get_ui_context().logger()
				std::cout
					<< "NA\tNA\t" ;	
			}
			std::cout << "\n" ;
		}
	}
	
private:
	StringStringMap m_pathway_names ;
	StringStringSetMap m_pathway_members ;
	StringStringSetMap m_clusters ;
	StringStringMap m_cluster_descriptions ;
	StringStringMap m_cluster_mapping ;
	StringSet m_universe ;
	StringSet m_test_genes ;
} ;

int main( int argc, char **argv ) {
	try {
		OverrepApplication app( argc, argv ) ;	
		app.process() ;
	}
	catch( appcontext::HaltProgramWithReturnCode const& e ) {
		return e.return_code() ;
	}
	return 0 ;
}
