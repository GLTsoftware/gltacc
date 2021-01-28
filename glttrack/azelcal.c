#include <math.h>
#include "track.h"

#define ARCSEC_TO_RAD 4.84813681e-6
extern double LATITUDE_DEGREES;

void azelcal(double *lst,double *ra,double *dec,double *az,double *el)
{
	double ha,cosphi;
	double  dpi =   4.0 * atan(1.0);                /* pi */
	double  dtupi = 2.0 * dpi;
	double phi;
	double cosh,sinh,cosd,sind,sinphi;
	double sina,cosa;
	double rad;

	rad = dpi / 180.0;

	phi = LATITUDE_DEGREES*rad;

	ha = *lst - *ra ;
	sinphi = sin(phi);
	cosphi = cos(phi);

	cosh = cos(ha);
	sinh = sin(ha);
	cosd = cos(*dec);
	sind = sin(*dec);

/*      azimuth and elevation   */

	sina = - cosd * sinh;

	cosa = sind * cosphi - cosd * cosh * sinphi;

	*el = asin(sind*sinphi + cosd*cosphi*cosh);

	if ( cosa == 0.0) {*az = 0.0;}
	else {*az = atan2(sina,cosa);}

	if(*az < 0.0) {*az = dtupi + *az;}
}
