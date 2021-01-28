#include <stdio.h>
#include <math.h>
#include <string.h>
#include "novas.h"
#include "track.h"

int first_time=1;

void
main(int *argc, char **argv)
{

	double          ra, dec;
	double          distance;
	double          Ra0, Dec0;
	body 	Object,From;
	site_info       geo_loc = {LATITUDE_DEGREES,LONGITUDE_DEGREES,
					HEIGHT_M,0.0,0.0};
	double tjd=2452115.0;

	double          pi;
	double delta=0.;

	Object.type=0;
	Object.number=atoi(argv[1]);
	strcpy(Object.name,"MERCURY");

	From.type=0;
	From.number=3;
	strcpy(From.name,"Earth");

	pi = 4.0 * atan(1.0);

	topo_planet(tjd, &Object,&From, delta, &geo_loc, &Ra0, &Dec0, &distance);

	printf("Got ra,dec: %f %f\n",Ra0,Dec0);

}

