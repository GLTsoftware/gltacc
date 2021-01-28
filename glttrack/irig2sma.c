/*	Title: 		IRIG-B Time to SMA Time Converter
	Purpose: 	This function converts IRIG-B time and encoder
			counts to SMA style units.
	Date:		06/26/98
	Author:		pjp
	*/

#include "vme_sg.h"
#include "sma_pt.h"
int irig_sma(POS_TIME *irig_pt, SMA_PT *sma_pt, short day_type)
{

	DAY_TYPE term_a, term_b, term_c, term_d, term_e, term_f, tmp_day;
	long tmp_hour;
	MSEC_TYPE tmp_msec;

	/* Position Conversion */
	sma_pt->az = (long )(308.990478*((double )irig_pt->az));
	sma_pt->el = (long )(308.990478*((double )irig_pt->el));

	/* Time Conversion */
	sma_pt->year = 	irig_pt->years; 

	sma_pt->day = 	1000*(0xf & (irig_pt->days>>12)) + 
			100*(0xf & (irig_pt->days>>8)) + 
			10*(0xf & (irig_pt->days>>4)) +
			1*(0xf & (irig_pt->days));

	sma_pt->hour = 	1000*(0xf & (irig_pt->hours>>12)) + 
			100*(0xf & (irig_pt->hours>>8)) + 
			10*(0xf & (irig_pt->hours>>4)) +
			1*(0xf & (irig_pt->hours));

	sma_pt->minute =1000*(0xf & (irig_pt->minutes>>12)) + 
			100*(0xf & (irig_pt->minutes>>8)) + 
			10*(0xf & (irig_pt->minutes>>4)) +
			1*(0xf & (irig_pt->minutes));

	sma_pt->usecond=10000000*(0xf & (irig_pt->useconds>>28)) + 
			1000000*(0xf & (irig_pt->useconds>>24)) + 
			100000*(0xf & (irig_pt->useconds>>20)) +
			10000*(0xf & (irig_pt->useconds>>16)) +
			1000*(0xf & (irig_pt->useconds>>12)) + 
			100*(0xf & (irig_pt->useconds>>8)) + 
			10*(0xf & (irig_pt->useconds>>4)) +
			1*(0xf & (irig_pt->useconds));



	/* Calc Julian Day Number for 12 hours GMT */
	term_a = (long )(((double )(sma_pt->year - 1))/100.0);
	term_b = (long )(((double )term_a)/4.0);
	term_c = 2 - term_a + term_b;
	term_d = sma_pt->day;
	term_e = (long )(365.25*(double )(sma_pt->year + 4715));
	term_f = 428;
	tmp_day = term_c + term_d + term_e + term_f - 1524;

	/* Find hour in the Julian Day */
	if (sma_pt->hour >= 12) tmp_hour = sma_pt->hour - 12;
	else {tmp_hour = sma_pt->hour + 12;tmp_day--;}

	if (day_type == GMT_DAY) { tmp_day = sma_pt->day;tmp_hour = sma_pt->hour; }

	/* Find Millisec of the day */
	tmp_msec = (long )(3600000.0*(double )(tmp_hour));
	tmp_msec = (long )(60000.0*(double )(sma_pt->minute)) + tmp_msec;
	tmp_msec = (long )(0.001*(double )(sma_pt->usecond)) + tmp_msec;

	/*
	tmp_msec += adjust;
	if (tmp_msec < 0){ tmp_msec += 86400000;tmp_day--;}
	else if (tmp_msec > 86400000){tmp_msec -= 86400000;tmp_day++;}
	*/
	sma_pt->msec = tmp_msec;
	sma_pt->day = tmp_day;
	sma_pt->hour = tmp_hour;
	}
