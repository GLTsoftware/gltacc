/* this file is track.h */
/* NAP 10 June 98 */
/* for mapping in az and el */

/* previously, these were defined as #defines, now
passing through reflective memory written by map (cvi) */
#define RM_MAP_NMAP                     0x811128
#define RM_MAP_AZ_CENTER                0x81112a
#define RM_MAP_EL_CENTER                0x81112c
#define RM_MAP_MAP_UNIT                 0x81112e
#define RM_MAP_SLEW_WAIT                0x811130


#define ADC_OFFSET 32642.5
#define ADC_SCALE 3296.14

/*
#define AZ_NEG_HARDSTOP_CTS 3028420
*/
#define AZ_NEG_HARDSTOP_CTS 2097180     /* main azi encoder counts
					for negative hardstop.
					Used by track.c in the logic
					for azi commanded position to
					avoid the cable-wrap-up 
					2097180 => -180 deg. */

#define INCREMENT 2
#define ACK '\006'
#define TRACKING_ERROR_THRESHOLD 0.5	/* degrees slew vs track mode */
#define EL_NEG_LIMIT 104858	/* 9 degrees (in counts) */
#define EL_POS_LIMIT 1025288	/* 88 degrees (in counts) */
#define TIME_BASE_ADDR 0x8000
#define	DTA          32.184	/* this is delta-T: diff betn TA and UTC */
#define	K            1.657e-3	/* K, EB, M0 and M1 are constants used */
#define	EB     	     1.671e-2	/* in calculations of et */
#define	M0         6.2399960
#define	M1 	     1.99096871e-7
#define	LEAPS	    32		/* leapseconds */
#define TIME_STEP   1.0
#define BIG_TIME_STEP 30
#define SEC_PER_DAY 86400
#define RESOLUTION 11650.8444	/* resolution of encoders */
#define FUTURE 0.0
#define ANTENNA_NODE_ID     1	/* reflective memory node id */
#define DERS_RA1_TILT1 1539
#define DERS_RA1_TILT2 1547
#define SCALE_FACTOR_TILT1 200.5
#define SCALE_FACTOR_TILT2 201.8
#define DEFAULT_INTEGRATION_TIME 50 /*msec for camera, sec for pos. sw.*/
#define ENABLED                         0x1
#define DISABLED                        0x0
#define ONSOURCE                        1
#define OFFSOURCE                      -1
#define INVALID                         0
#define RM_BLANKING_SOURCE              0x01113e
#define RM_BLANKING_CHOPPER             0x01114e
#define SMOOTH_LENGTH			2	
#define CHOPPER_INTEGRATION		5 /* 1 second chopper position/sw*/
#define ARCSEC_TO_RAD 4.84813681e-6
#define AZ_TRAILER_TX 326.507063
#define EL_TRAILER_TX 7.213897
#define AZ_TOWER_LIGHT 331.967698
#define EL_TOWER_LIGHT 13.786811
#define AZ_BORESIGHT 337.26
#define EL_BORESIGHT 13.993
#define CHOPPER_BEAM 55.
#define OK 0
#define ERROR -1
#define RUNNING 1
 
#define SUNLIMIT 25.
