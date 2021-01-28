#include "ephem_read.h"
#define EMRAT 81.30056  /* Earth-Moon mass ratio */
#define KMPERAU 1.49597892e8 /*  Astronomical Unit in Km */
void jplint(double *tjd, long int *targ, long int *cent,
	 double *posvel, long int *err_flg)
{  
char ephemFileName[12];
stateType State;
int ntarg,ncent,i,j,list[13];
extern int first_time;
double pv[6][13],pvsun[6];

ntarg = *targ - 1;
ncent = *cent - 1;


strcpy(ephemFileName,"binEphem.405");

  for(i=0;i<6;i++)  posvel[i]=0.0;
  
  if(first_time) 
    {
      Initialize_Ephemeris(ephemFileName);
      first_time=0;
    }

  for(i=0;i<13;i++) list[i] = 0.0;
  
  for(i=0;i<2;i++)
    {
      j = ntarg;
      if(i == 1) j = ncent;
      if(j <= 9) list[j] = 1;
      if(j == 9) list[2] = 1;
      if(j == 2) list[9] = 1;
      if(j == 12) list[2] = 1;
    }

  Interpolate_State(*tjd,10,&State);

  for(i=0;i<3;i++)
    {
      pvsun[i]=State.Position[i]/KMPERAU;
      pvsun[i+3]=State.Velocity[i]/KMPERAU;
    }

  for(i=0;i<13;i++)
    {
      if(list[i] != 0) 
	{
	  Interpolate_State(*tjd,i,&State);
	  for(j=0;j<3;j++)
	    {
	      pv[j][i]=State.Position[j]/KMPERAU;
	      pv[j+3][i]=State.Velocity[j]/KMPERAU;
	    }
	}
    }

  if ((ntarg == 10) || (ncent == 10)) for(i=0;i<6;i++) pv[i][10]=pvsun[i];
 
  if ((ntarg == 11) || (ncent == 11)) for(i=0;i<6;i++) pv[i][11]=0.0;

  if ((ntarg == 12) || (ncent == 12)) for(i=0;i<6;i++) pv[i][12]=pv[i][2];

  if ((ntarg*ncent == 18) && ((ntarg+ncent) == 11)) 
    for(i=0;i<6;i++) pv[i][3]=0.0;

  if (list[2] == 1) for(i=0;i<6;i++) pv[i][2]=pv[i][2]-pv[i][9]/(1.0+EMRAT);

  if (list[9] == 1) for(i=0;i<6;i++) pv[i][9]=pv[i][2]+pv[i][9];

  for(i=0;i<6;i++) posvel[i]=pv[i][ntarg]-pv[i][ncent];

}
