parse_bitpack_haplotypes <-
function( haps, N ) {
   # stopifnot( length( haps ) == 2*N ) ;
	result = as.integer( haps )
	result[ which( result == 255 ) ] = NA
    return( result )
}
