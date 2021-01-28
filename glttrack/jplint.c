#include "ephem_read.h"

void jplint (double *tjd, long int *targ, long int *cent,
	 double *posvel, long int *err_flg)
{
	char ephemFileName[12];
	double Time=2452115.0;
	stateType State1,State2;
	int i;

	strcpy(ephemFileName,"binEphem.405");

	Initialize_Ephemeris(ephemFileName);
	Interpolate_State(*tjd,(*targ)-1,&State1);
/*
	Interpolate_State(Time,(*cent)-1,&State2);
*/
	for(i=0;i<3;i++)
	{

	posvel[i]=State1.Position[i]/1.49597892e8;
	posvel[i+3]=State1.Velocity[i]/1.49597892e8;
/*
	printf("Velocity[0]=%lf\n",posvel[3]);
	printf("Velocity[1]=%lf\n",posvel[4]);
 	printf("Velocity[2]=%lf\n",posvel[5]);
	printf("%lf %lf\n",State1.Position[i]-State2.Position[i],State1.Velocity[i]-State2.Velocity[i]);
	posvel[i]=(State1.Position[i]-State2.Position[i])/1.49597892e8;
	posvel[i+3]=(State1.Velocity[i]-State2.Velocity[i])/1.49597892e8;
*/
	}
	printf("Target=%d Center=%d Position=%.12e %.12e %.12e\n",(*targ-1),(*cent-1),posvel[0],posvel[1],posvel[2]);
	
	*err_flg=0;

}
