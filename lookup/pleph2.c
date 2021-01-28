#include <stdio.h>
#include "SpiceUsr.h"
extern double tjd2et(double tjd);
void jplint(double *tjd, long int *targ,long int *cent,double *posvel)
{

   #define     FILE_SIZE 128
   #define     WORD_SIZE 80
	
	int i;
	extern int first_time_spk;

   SpiceDouble state[6];
   SpiceDouble lt;
   SpiceDouble Et;

   SpiceChar   leap[FILE_SIZE];
   SpiceChar   spk1 [FILE_SIZE];
   SpiceChar   spk2 [FILE_SIZE];
   SpiceChar   spk3 [FILE_SIZE];
   SpiceChar   spk4 [FILE_SIZE];
   SpiceChar   spk5 [FILE_SIZE];
   SpiceChar   spk6 [FILE_SIZE];
   SpiceChar   spk7 [FILE_SIZE];
   SpiceChar   spk8 [FILE_SIZE];
   SpiceChar   starg[WORD_SIZE];
   SpiceChar   obs [WORD_SIZE];

strcpy(leap,"./catalogs/time.ker");
strcpy(spk1,"./catalogs/de405_1998_2050.bsp");
strcpy(spk2,"./catalogs/saturnWithMoons.bsp");
strcpy(spk3,"./catalogs/jupiter.bsp");
strcpy(spk4,"./catalogs/neptune.bsp");
strcpy(spk5,"./catalogs/asteroids.bsp");
strcpy(spk6,"./catalogs/comets.bsp");
strcpy(spk7,"./catalogs/asteroidyu55.bsp");
strcpy(spk8,"./catalogs/asteroid1998qe2.bsp");
if(*targ==375) *targ=2000001;
if(*targ==376) *targ=2000002;
if(*targ==377) *targ=2000010;
if(*targ==378) *targ=2000004;
if(*targ==386) *targ=1003291;
if(*targ==387) *targ=1003203;

sprintf(starg,"%d",*targ);
sprintf(obs,"%d",*cent); 

if(first_time_spk)
{
   furnsh_c ( leap );
   furnsh_c ( spk1  );
   furnsh_c ( spk2  );
   furnsh_c ( spk3  );
   furnsh_c ( spk4  );
   furnsh_c ( spk5  );
   furnsh_c ( spk6  );
   furnsh_c ( spk7  );
   furnsh_c ( spk8  );
first_time_spk=0;
}
            Et = tjd2et(*tjd);

   spkezr_c (  starg, Et, "J2000", "NONE", obs, state, &lt );

	for(i=0;i<6;i++) {
	posvel[i]=(double)state[i];
	}
}
