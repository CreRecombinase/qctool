load.genotypes <-
function(
	hapdb,
	chromosome = NULL, range = NULL,
	positions = NULL, variant_ids = NULL, rsids = NULL,
	samples = NULL, analysis = NULL,
	verbose = FALSE,
	compute.dosage = FALSE, compute.probabilities = TRUE,
	method = "cpp",
	sample.identifier.column = NULL
) {
	suppressMessages(require( RSQLite ))
	suppressMessages(require( Rcompression ))
	if( is.null( sample.identifier.column )) {
		sample.identifier.column = colnames( hapdb$samples )[ -which( colnames( hapdb$samples ) %in% c( "analysis", "analysis_id", "index_in_data" ) ) ][1]
	}
	if( is.null( analysis ) ) {
		analysis = as.character( unique( hapdb$samples$analysis ) )
		if( length( analysis ) > 1 ) {
			stop( "This hapdb file has more than one analysis, please specify one." ) ;
		} else if( length( analysis ) == 0 ) {
			stop( "This hapdb file seems to have no analyses!" ) ;
		}
	}
	
	analysis_id = hapdb$samples$analysis_id[ match( analysis, hapdb$samples$analysis ) ]
	
	dbGetQuery( hapdb$db, "CREATE TEMPORARY TABLE tmpHapdbLoadGenotypes ( variant_id INT NOT NULL )" ) ;
	if( !is.null( chromosome ) && !is.null( range )) {
		dbGetQuery( hapdb$db, sprintf( "INSERT INTO tmpHapdbLoadGenotypes SELECT id FROM Variant WHERE chromosome == '%s' AND position BETWEEN %d AND %d ORDER BY chromosome, position, rsid, alleleA, alleleB", chromosome, range[1], range[2] ) ) ;
    }
	if( !is.null( rsids ) ) {
		dbGetPreparedQuery( hapdb$db, "INSERT INTO tmpHapdbLoadGenotypes SELECT id FROM Variant WHERE rsid == ?", data.frame( rsid = rsids )) ;
	}
    if( !is.null( positions )) {
		dbGetPreparedQuery( hapdb$db, "INSERT INTO tmpHapdbLoadGenotypes SELECT id FROM Variant WHERE chromosome == ? AND position == ? ORDER BY rsid, alleleA, alleleB", positions ) ;
    }
    if( !is.null( variant_ids )) {
		dbGetPreparedQuery( hapdb$db, "INSERT INTO tmpHapdbLoadGenotypes VALUES(?)", data.frame( variant_id = variant_ids ) ) ;
    }

	sql = paste(
		"SELECT analysis_id, E.name AS analysis, tmp.variant_id AS variant_id, chromosome, position, rsid, alleleA, alleleB, H.N AS N, H.data AS data",
		"FROM tmpHapdbLoadGenotypes tmp",
		"INNER JOIN Analysis E",
		sprintf( "  ON E.id == %s", analysis_id ),
		"INNER JOIN Variant V ON V.id == tmp.variant_id",
		"INNER JOIN Genotype H ON H.analysis_id == E.id AND H.variant_id == tmp.variant_id",
		"ORDER BY tmp.rowid",
		sep = " "
	)

	if( verbose ) {
		cat( "load.genotypes(): running query :\"", sql, "\"...\n", sep = "" ) ;
		print( dbGetQuery( hapdb$db, sprintf( "EXPLAIN QUERY PLAN %s", sql ) ) )
	}

	D = dbGetQuery( hapdb$db, sql )
    dbGetQuery( hapdb$db, "DROP TABLE tmpHapdbLoadGenotypes" ) ;

	# Get all the samples for this analysis
	all.samples = hapdb$samples[ which( hapdb$samples$analysis == analysis ), ]
	N = nrow( all.samples )
	samples.choice = 1:N
	if( !is.null( samples ) ) {
		if( mode( samples ) == "character" ) {
			samples.choice = sort( which( hapdb$samples$analysis == analysis & hapdb$samples[, sample.identifier.column ] %in% samples ) ) ;
		} else {
			samples.choice = samples
			stopifnot( length( which( samples.choice %in% 1:N ) ) == 0 ) ;
		}
	}
	# Get the samples chosen by the user.  Also compute indices of genotype call probabilities for these samples.
	chosen.samples = all.samples[ samples.choice, ]
	genotypes.choice = sort( c( (samples.choice * 3) - 2, (samples.choice * 3) - 1, (samples.choice * 3) ) )

	if( nrow(D) > 0 ) {
    	if( verbose ) {
    		cat( "load.genotypes(): loaded, uncompressing...\n" ) ;
        }
		n = nrow( chosen.samples )
		result = list(
			samples = chosen.samples,
			variant = D[,-which( colnames(D) == "data")]
		)
		
		if( method == "cpp" ) {
			if( verbose ) {
				cat( "samples.choice =\n" )
				print( samples.choice )
			}
			X = rcpp_uncompress_floatarray_genotypes( D$data, N, samples.choice, compute.probabilities, compute.dosage ) ;
			if( compute.probabilities ) {
				result$data = X$probabilities ;
			}
			if( compute.dosage ) {
				result$dosage = X$dosage ;
			}
		} else {
			if( compute.dosage ) {
				result$dosage = matrix( NA, nrow = nrow( D ), ncol = n ) ;
			}
			if( compute.probabilities ) {
				result$data = matrix( NA, nrow = nrow(D), ncol = 3*n )
			}
			for( i in 1:nrow(D) ) {
				if( verbose ) {
					cat( "uncompressing", i, "of", nrow(D), "...\n" ) ;
				}
				compressed_data = unlist( D$data[i] )
				uncompressed_data = uncompress( compressed_data, asText = FALSE )
				v = parse_genotypes( uncompressed_data, D$N[i] )
				# Get rid of unwanted samples
				v = v[ genotypes.choice ]
				# Handle the case of three zero probabilities
				# This should not happen in newer files, but an early version of this code in qctool produced these.
				wMissing = which( pmax( v[ seq( from = 1, length = n, by = 3 ) ], v[ seq( from = 2, length = n, by = 3 ) ], v[ seq( from = 3, length = n, by = 3 ) ] ) == 0 )
				v[wMissing*3 - 2] = NA
				v[wMissing*3 - 1] = NA
				v[wMissing*3] = NA
				if( compute.probabilities ) {
					result$data[i,] = v
				}
				if( compute.dosage ) {
					result$dosage[i,] = v[ seq( from = 2, length = n, by = 3 ) ] + 2 * v[ seq( from = 3, length = n, by = 3 ) ]
				}
				if( i == nrow(D) || i %% 10 == 0 ) {
					rm( v ) ;
					rm( compressed_data ) ;
					rm( uncompressed_data ) ;
					gc()
				}
			}
		}
		if( compute.probabilities ) {
			colnames( result$data ) = rep( NA, 3*n )
			colnames( result$data )[ seq( from = 1, by = 3, length = n ) ] = paste( result$samples[, sample.identifier.column ], "AA", sep = ":" )
			colnames( result$data )[ seq( from = 2, by = 3, length = n ) ] = paste( result$samples[, sample.identifier.column ], "AB", sep = ":" )
			colnames( result$data )[ seq( from = 3, by = 3, length = n ) ] = paste( result$samples[, sample.identifier.column ], "BB", sep = ":" )
			rownames( result$data) = result$variant$rsid
		}
		if( compute.dosage ) {
			colnames( result$dosage ) = result$samples[, sample.identifier.column ]
			rownames( result$dosage ) = result$variant$rsid
		}
	} else {
		result = list(
			samples = chosen.samples,
			variant = D[,-which( colnames(D) == "data")]
		) ;
		n = nrow( result$samples )
		if( compute.probabilities ) {
			result$data = matrix( NA, nrow = 0, ncol = 3 * n )
			colnames( result$data ) = rep( NA, 3 * n )
			colnames( result$data )[ seq( from = 1, by = 3, length = n ) ] = paste( result$samples[, sample.identifier.column ], "AA", sep = ":" )
			colnames( result$data )[ seq( from = 2, by = 3, length = n) ] = paste( result$samples[, sample.identifier.column ], "AB", sep = ":" )
			colnames( result$data )[ seq( from = 3, by = 3, length = n ) ] = paste( result$samples[, sample.identifier.column ], "BB", sep = ":" )
		}
		if( compute.dosage ) {
			result$dosage = matrix( NA, nrow = 0, ncol = n )
			colnames( result$dosage ) = result$samples[, sample.identifier.column ]
		}
	}

	if( verbose ) {
		cat( "load.genotypes(): done.\n" ) ;
	}
	return( result )
}

