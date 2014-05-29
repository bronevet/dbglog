/* msr_rapl.c
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "msr_core.h"
#include "msr_rapl.h"

/* 
 * Macros 
 */

/* MASK_RANGE
 * Create a mask from bit m to n. 
 * 63 >= m >= n >= 0
 * Example:  MASK_RANGE(4,2) -->     (((1<<((4)-(2)+1))-1)<<(2)) 
 * 				     (((1<<          3)-1)<<(2))
 * 				     ((               4-1)<<(2))
 * 				     (                  3)<<(2))
 * 				     (                       24) = b11000
 */ 				 
#define MASK_RANGE(m,n) ((((uint64_t)1<<((m)-(n)+1))-1)<<(n))	// m>=n

/* MASK_VAL
 * Return the value of x after applying bitmask (m,n).
 * 63 >= m >= n >= 0
 * Example:  MASK_RANGE(17,4,2) --> 17&24 = b10001 & b11000 = b10000
 */
#define MASK_VAL(x,m,n) (((uint64_t)(x)&MASK_RANGE((m),(n)))>>(n))

/* UNIT_SCALE 
 * Calculates x/(2^y).  
 * Example:  The RAPL interface measures power in units of 1/(2^p) watts,
 * where is expressed as bits 0:3 of the RAPL_POWER_UNIT MSR, with the
 * default value y=b011 (thus 1/8th of a Watt).  Reading a power values
 * of x=b10000 translates into UNIT_SCALE(x,y) Watts, and a power cap of 
 * x Watts translates to a representation of UNIT_DESCALE(x,y) units.
 *
 */
#define UNIT_SCALE(x,y) ((x)/(double)(1<<(y)))
#define UNIT_DESCALE(x,y) ((x)*(double)(1<<(y)))




// Section 35.7
// Table 35-11.  MSRs supported by Intel processors based on Intel 
// microarchitecture code name Sandy Bridge.
// Model/family 06_2A, 06_2D.
#if (USE_062A || USE_062D)
#define MSR_RAPL_POWER_UNIT 		(0x606)	// ro
#define MSR_PKG_POWER_LIMIT 		(0x610) // rw
#define MSR_PKG_ENERGY_STATUS 		(0x611) // ro sic;  MSR_PKG_ENERY_STATUS
#define MSR_PKG_POWER_INFO 		(0x614) // rw (text states ro)
#define MSR_PP0_POWER_LIMIT 		(0x638) // rw
#define MSR_PP0_ENERY_STATUS 		(0x639) // ro
#define MSR_PP0_POLICY 			(0x63A) // rw
#define MSR_PP0_PERF_STATUS 		(0x63B) // ro
#endif

// Section 35.7.1
// Table 35-12. MSRs supported by second generation Intel Core processors 
// (Intel microarchitecture Code Name Sandy Bridge)
// Model/family 06_2AH
#if (USE_062A)
#define MSR_PP1_POWER_LIMIT 		(0x640) // rw
#define MSR_PP1_ENERGY_STATUS 		(0x641)	// ro.  sic; MSR_PP1_ENERY_STATUS
#define MSR_PP1_POLICY 			(0x642) // rw
#endif

// Section 35.7.2
// Table 35-13. Selected MSRs supported by Intel Xeon processors E5 Family 
// (based on Intel Microarchitecture code name Sandy Bridge) 
// Model/family 06_2DH
#if (USE_062D)
#define MSR_PKG_PERF_STATUS 		(0x613) // ro
#define MSR_DRAM_POWER_LIMIT 		(0x618) // rw	
#define MSR_DRAM_ENERGY_STATUS 		(0x619)	// ro.  sic; MSR_DRAM_ENERY_STATUS
#define MSR_DRAM_PERF_STATUS 		(0x61B) // ro
#define MSR_DRAM_POWER_INFO 		(0x61C) // rw (text states ro)
#endif

// Section 35.8.1
// Table 35-15. Selected MSRs supported by Intel Xeon processors E5 Family v2 
// (based on Intel microarchitecture code name Ivy Bridge) 
// Model/family 06_3EH.  
// The Intel documentation only lists this table of msrs; this may be an error.
#if (USE_063E)
#define MSR_PKG_PERF_STATUS 		(0x613) //
#define MSR_DRAM_POWER_LIMIT 		(0x618) //
#define MSR_DRAM_ENERGY_STATUS 		(0x619)	// sic; MSR_DRAM_ENERY_STATUS
#define MSR_DRAM_PERF_STATUS 		(0x61B) //
#define MSR_DRAM_POWER_INFO 		(0x61C) //
#endif

enum{
	BITS_TO_WATTS,
	WATTS_TO_BITS,
	BITS_TO_SECONDS,
	SECONDS_TO_BITS,
	BITS_TO_JOULES,
	JOULES_TO_BITS,
	NUM_XLATE
};

struct rapl_units{
	uint64_t msr_rapl_power_unit;	// raw msr value
	double seconds;
	double joules;
	double watts;
};

static void
translate( const int socket, uint64_t* bits, double* units, int type ){
	static int initialized=0;
	static struct rapl_units ru[NUM_SOCKETS];
	uint64_t val[NUM_SOCKETS];
	int i;
	if(!initialized){
		initialized=1;
		read_all_sockets( MSR_RAPL_POWER_UNIT, val );
		for(i=0; i<NUM_SOCKETS; i++){
			// See figure 14-16 for bit fields.
			//  1  1 1  1 1 
			//  9  6 5  2 1  8 7  4 3  0
			//
			//  1010 0001 0000 0000 0011
			//
			//     A    1    0    0    3
			//ru[i].msr_rapl_power_unit = 0xA1003;

			ru[i].msr_rapl_power_unit = val[i];
			// default is 1010b or 976 microseconds
			ru[i].seconds = 1.0/(double)( 1<<(MASK_VAL( ru[i].msr_rapl_power_unit, 19, 16 )));
			// default is 10000b or 15.3 microjoules
			ru[i].joules  = 1.0/(double)( 1<<(MASK_VAL( ru[i].msr_rapl_power_unit, 12,  8 )));
			// default is 0011b or 1/8 Watts
			ru[i].watts   = ((1.0)/((double)( 1<<(MASK_VAL( ru[i].msr_rapl_power_unit,  3,  0 )))));
		}	
	}
	switch(type){
		case BITS_TO_WATTS: 	*units = (double)(*bits)  * ru[socket].watts; 			break;
		case BITS_TO_SECONDS:	*units = (double)(*bits)  * ru[socket].seconds; 		break;
		case BITS_TO_JOULES:	*units = (double)(*bits)  * ru[socket].joules; 		break;
		case WATTS_TO_BITS:	*bits  = (uint64_t)(  (*units) / ru[socket].watts    ); 	break;
		case SECONDS_TO_BITS:	*bits  = (uint64_t)(  (*units) / ru[socket].seconds  ); 	break;
		case JOULES_TO_BITS:	*bits  = (uint64_t)(  (*units) / ru[socket].joules   ); 	break;
		default: 
			fprintf(stderr, "%s:%d  Unknown value %d.  This is bad.\n", __FILE__, __LINE__, type);  
			*bits = -1;
			*units= -1.0;
			break;
	}
}

struct rapl_power_info{
	uint64_t msr_pkg_power_info;	// raw msr values
	uint64_t msr_dram_power_info;

	double pkg_max_power;		// watts
	double pkg_min_power;
	double pkg_max_window;		// seconds
	double pkg_therm_power;		// watts

	double dram_max_power;		// watts
	double dram_min_power;
	double dram_max_window;		// seconds
	double dram_therm_power;	// watts
};

static void
get_rapl_power_info( const int socket, struct rapl_power_info *info){
	uint64_t val = 0;
	//info->msr_pkg_power_info  = 0x6845000148398;
	//info->msr_dram_power_info = 0x682d0001482d0;

	read_msr_by_coord( socket, 0, 0, MSR_PKG_POWER_INFO, &(info->msr_pkg_power_info) );
#ifdef RAPL_DRAM_AVAIL
	read_msr_by_coord( socket, 0, 0, MSR_DRAM_POWER_INFO, &(info->msr_dram_power_info) );
#endif
	// Note that the same units are used in both the PKG and DRAM domains.
	
	val = MASK_VAL( info->msr_pkg_power_info,  53, 48 );
	translate( socket, &val, &(info->pkg_max_window), BITS_TO_SECONDS );
	
	val = MASK_VAL( info->msr_pkg_power_info,  46, 32 );
	translate( socket, &val, &(info->pkg_max_power), BITS_TO_WATTS );

	val = MASK_VAL( info->msr_pkg_power_info,  30, 16 );
	translate( socket, &val, &(info->pkg_min_power), BITS_TO_WATTS );

	val = MASK_VAL( info->msr_pkg_power_info,  14,  0 );
	translate( socket, &val, &(info->pkg_therm_power), BITS_TO_WATTS );

	val = MASK_VAL( info->msr_dram_power_info, 53, 48 );
	translate( socket, &val, &(info->dram_max_window), BITS_TO_SECONDS );

	val = MASK_VAL( info->msr_dram_power_info, 46, 32 );
	translate( socket, &val, &(info->dram_max_power), BITS_TO_WATTS );

	val = MASK_VAL( info->msr_dram_power_info, 30, 16 );
	translate( socket, &val, &(info->dram_min_power), BITS_TO_WATTS );

	val = MASK_VAL( info->msr_dram_power_info, 14,  0 );
	translate( socket, &val, &(info->dram_therm_power), BITS_TO_WATTS );
}

static void
calc_rapl_limit(const int socket, struct rapl_limit* limit1, struct rapl_limit* limit2, struct rapl_limit* dram ){
	static struct rapl_power_info rpi[NUM_SOCKETS];
	static int initialized=0;
	uint64_t watts_bits=0, seconds_bits=0;
	int i;
	if(!initialized){
		initialized=1;
		for(i=0; i<NUM_SOCKETS; i++){
			get_rapl_power_info(i, &(rpi[i]));
		}
	}
	if(limit1){
		if (limit1->bits){
			// We have been given the bits to be written to the msr.
			// For sake of completeness, translate these into watts 
			// and seconds.
			watts_bits   = MASK_VAL( limit1->bits, 14,  0 );
			seconds_bits = MASK_VAL( limit1->bits, 23, 17 );

			translate( socket, &watts_bits, &limit1->watts, BITS_TO_WATTS );
			translate( socket, &seconds_bits, &limit1->seconds, BITS_TO_SECONDS );

		}else{
			// We have been given watts and seconds and need to translate
			// these into bit values.
			translate( socket, &watts_bits,   &limit1->watts,   WATTS_TO_BITS   );
			translate( socket, &seconds_bits, &limit1->seconds, SECONDS_TO_BITS );
			limit1->bits |= watts_bits   << 0;
			limit1->bits |= seconds_bits << 17;
		}
	}	
	if(limit2){
		if (limit2->bits){
			watts_bits   = MASK_VAL( limit2->bits, 46, 32 );
			seconds_bits = MASK_VAL( limit2->bits, 55, 49 );

			translate( socket, &watts_bits, &limit2->watts, BITS_TO_WATTS );
			translate( socket, &seconds_bits, &limit2->seconds, BITS_TO_SECONDS );

		}else{
			translate( socket, &watts_bits,   &limit2->watts,   WATTS_TO_BITS   );
			translate( socket, &seconds_bits, &limit2->seconds, SECONDS_TO_BITS );
			limit2->bits |= watts_bits   << 32;
			limit2->bits |= seconds_bits << 49;
		}
	}
	if(dram){
		if (dram->bits){
			// We have been given the bits to be written to the msr.
			// For sake of completeness, translate these into watts 
			// and seconds.
			watts_bits   = MASK_VAL( dram->bits, 14,  0 );
			seconds_bits = MASK_VAL( dram->bits, 23, 17 );

			translate( socket, &watts_bits, &dram->watts, BITS_TO_WATTS );
			translate( socket, &seconds_bits, &dram->seconds, BITS_TO_SECONDS );

		}else{
			// We have been given watts and seconds and need to translate
			// these into bit values.
			translate( socket, &watts_bits,   &dram->watts,   WATTS_TO_BITS   );
			translate( socket, &seconds_bits, &dram->seconds, SECONDS_TO_BITS );
			dram->bits |= watts_bits   << 0;
			dram->bits |= seconds_bits << 17;
		}
	}
}

void
dump_rapl_limit( struct rapl_limit* L ){
	fprintf(stdout, "bits    = %lx\n", L->bits);
	fprintf(stdout, "seconds = %lf\n", L->seconds);
	fprintf(stdout, "watts   = %lf\n", L->watts);
	fprintf(stdout, "\n");
}

void 
set_rapl_limit( const int socket, struct rapl_limit* limit1, struct rapl_limit* limit2, struct rapl_limit* dram ){
	// Fill in whatever values are necessary.
	uint64_t pkg_limit=0;
#ifdef RAPL_DRAM_AVAIL
	uint64_t dram_limit=0;
#endif 
	calc_rapl_limit( socket, limit1, limit2, dram );

	if(limit1){
		pkg_limit |= limit1->bits | (1LL << 15) | (1LL << 16);	// enable clamping
	}
	if(limit2){
		pkg_limit |= limit2->bits | (1LL << 47) | (1LL << 48);	// enable clamping
	}
	if(limit1 || limit2){
		write_msr_by_coord( socket, 0, 0, MSR_PKG_POWER_LIMIT, pkg_limit );
	}
#ifdef RAPL_DRAM_AVAIL
	if(dram){
		dram_limit |= dram->bits | (1LL << 15) | (1LL << 16);	// enable clamping
		write_msr_by_coord( socket, 0, 0, MSR_DRAM_POWER_LIMIT, dram_limit );
	}
#endif
}

void 
get_rapl_limit( const int socket, struct rapl_limit* limit1, struct rapl_limit* limit2, struct rapl_limit* dram ){
	if(limit1){
		read_msr_by_coord( socket, 0, 0, MSR_PKG_POWER_LIMIT, &(limit1->bits) );
	}
	if(limit2){
		read_msr_by_coord( socket, 0, 0, MSR_PKG_POWER_LIMIT, &(limit2->bits) );
	}
#ifdef RAPL_DRAM_AVAIL
	if(dram){
		read_msr_by_coord( socket, 0, 0, MSR_DRAM_POWER_LIMIT, &(dram->bits) );
	}
#endif 
	// Fill in whatever values are necessary.
	calc_rapl_limit( socket, limit1, limit2, dram );
}

void
dump_rapl_terse_label(){
	int socket;
	for(socket=0; socket<NUM_SOCKETS; socket++){
		fprintf(stdout,"pkgW%02d dramW%02d ", socket, socket );
	}
}

void
dump_rapl_terse(){
	int socket;
	struct rapl_data r;

	for(socket=0; socket<NUM_SOCKETS; socket++){
		read_rapl_data(socket, &r);
		fprintf(stdout,"%8.4lf %8.4lf ", r.pkg_watts, r.dram_watts);
	}
}

void 
dump_rapl_data( struct rapl_data *r ){
	static int initialized=0;
	static struct timeval start;
	struct timeval now;
	if(!initialized){
		initialized=1;
		gettimeofday( &start, NULL );
	}
	gettimeofday( &now, NULL );
	fprintf(stdout, "pkg_watts= %8.4lf   elapsed= %8.5lf   timestamp= %9.6lf\n", 
			r->pkg_watts,
			r->elapsed,
			now.tv_sec - start.tv_sec + (now.tv_usec - start.tv_usec)/1000000.0
			);
	fprintf(stdout, "dram_watts= %8.4lf   elapsed= %8.5lf   timestamp= %9.6lf\n", 
			r->dram_watts,
			r->elapsed,
			now.tv_sec - start.tv_sec + (now.tv_usec - start.tv_usec)/1000000.0
			);
}

void
read_rapl_data( const int socket, struct rapl_data *r ){
	/* 
	 * If r is null, measurments are recorded into the local static struct s.
	 *
	 * If r is not null and r->flags == 0, measurements are also recorded 
	 *   into the local static struct s and the delta is recorded in r.
	 *
	 * If r is not null and r->flags & RDF_REENTRANT, measurements are recorded in
	 *   r only and the delta is calculated relative to the difference in 
	 *   the values passed in with r.  old_* values are destroyed and * values
	 *   are moved to old_* values.  
	 *
	 * If r is not null and r->flags & RDF_INITIALIZE, no calculations are 
	 *   performed and deltas are set to zero.  This is intented as a convenience
	 *   function only; there is no harm in initializing without this flag and
	 *   simply ignoring the results.
	 *
	 * This functionality allows for the case where the user wishes to, e.g., 
	 *   take measurments at points A, B, C, C', B', A' using three separate
	 *   rapl_data structs.
	 */

	struct rapl_data *p;	// Where the previous state lives.
	uint64_t  maxbits=4294967296;
	double max_joules=0.0;
	static struct rapl_data s[NUM_SOCKETS];

	if( (r == NULL) || !(r->flags & RDF_REENTRANT) ){
		p = &s[socket];
	} else{
		p = r;
/*		if(r->flags & RDF_INITIALIZE) {
			p->pkg_bits = 0;
			p->dram_bits = 0;
			p->pkg_joules = 0;
			p->dram_joules = 0;
			
		}*/
	}


	// Move current variables to "old" variables.
	p->pvt_old_pkg_bits	= p->pvt_pkg_bits;
	p->pvt_old_dram_bits	= p->pvt_dram_bits;
	p->pvt_old_pkg_joules	= p->pvt_pkg_joules;
	p->pvt_old_dram_joules	= p->pvt_dram_joules;
	p->pvt_old_now.tv_sec 	= p->pvt_now.tv_sec;
	p->pvt_old_now.tv_usec	= p->pvt_now.tv_usec;

	// Get current timestamp
	gettimeofday( &(p->pvt_now), NULL );

	// Get raw joules
	read_msr_by_coord( socket, 0, 0, MSR_PKG_ENERGY_STATUS,  &(p->pvt_pkg_bits)  );
#ifdef RAPL_DRAM_AVAIL
	read_msr_by_coord( socket, 0, 0, MSR_DRAM_ENERGY_STATUS, &(p->pvt_dram_bits) );
#endif
	
	// get normalized joules
	translate( socket, &(p->pvt_pkg_bits),  &(p->pvt_pkg_joules),  BITS_TO_JOULES );
	translate( socket, &(p->pvt_dram_bits), &(p->pvt_dram_joules), BITS_TO_JOULES );
	
	// Fill in the struct if present.
	if(r){
		// Get delta in seconds
		r->elapsed = (p->pvt_now.tv_sec - p->pvt_old_now.tv_sec) 
			     +
			     (p->pvt_now.tv_usec - p->pvt_old_now.tv_usec)/1000000.0;

		// Get delta joules.
		// Now handles wraparound.
//printf("p->pkg_joules=%le, p->old_pkg_joules=%le\n", p->pvt_pkg_joules , p->pvt_old_pkg_joules);
//printf("p->dram_joules=%le, p->old_dram_joules=%le\n", p->pvt_dram_joules , p->pvt_old_dram_joules);
		if(p->pvt_pkg_joules - p->pvt_old_pkg_joules < 0)
		{
			translate(socket, &maxbits, &max_joules, BITS_TO_JOULES); 
			r->pkg_joules = ( p->pvt_pkg_joules + max_joules) - p->pvt_old_pkg_joules;
		} else {
			r->pkg_joules  = p->pvt_pkg_joules  - p->pvt_old_pkg_joules;		
		}

		if(p->pvt_dram_joules - p->pvt_old_dram_joules < 0)
		{
			translate(socket, &maxbits, &max_joules, BITS_TO_JOULES); 
			r->dram_joules = (p->pvt_dram_joules + max_joules) - p->pvt_old_dram_joules;
		} else {
			r->dram_joules = p->pvt_dram_joules - p->pvt_old_dram_joules;	
		}	

		// Get watts.
		// Does not check for div by 0.
		r->pkg_watts  = r->pkg_joules  / r->elapsed;
		r->dram_watts = r->dram_joules / r->elapsed;
	}
}

