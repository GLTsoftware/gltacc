#ifndef _sma_pt
#define _sma_pt
/*
	Title:		SMA Position and Time
	Purpose:	Define SMA Position and Time
	Date:		07/02/98
	Author:		pjp
	*/

typedef long MSEC_TYPE;
typedef long DAY_TYPE;

typedef struct {
	long year; 
	DAY_TYPE day;
	long hour;
	long minute;
	long usecond;
	MSEC_TYPE msec;	/* millisecs of the day */
	long az;	/* milliarcsec */
	long el;	/* milliarcsec */
	} SMA_PT;

#define GMT_DAY 0

extern int irig_sma(POS_TIME *irig_pt, SMA_PT *sma_pt, short day_type);

#endif
