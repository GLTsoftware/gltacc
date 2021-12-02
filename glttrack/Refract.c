#include <math.h>
#include <stdlib.h>
#include <stdio.h>

extern void slaRefro ( double zobs, double hm, double tdk, double pmb,
                double rh, double wl, double phi, double tlr,
                double eps, double *ref );

void Refract (double *el, int radio_flag, double temp, double humid, double pres,float *refraction) {

double zobs,hm,tdk,pmb,rh,wl=1300.,phi,tlr,eps,ref;
double refractiond;

zobs = M_PI/2.0 - *el;

hm = 100.0; /* height of antenna pad in meters above sea level */

tdk = temp+273.15; /* ambient temp. in K , input is in C*/

pmb = pres;; /* pressure in mbar */

rh = humid/100.; /* relative humidity 0-1, input is in % */
   if(radio_flag==1) wl = 1300.0;
   if(radio_flag==0) wl = 0.5;

phi = 76.53525 * M_PI/180. ; /* latitude of pad 1 in radians */

tlr = 0.0065;
eps = 1.0e-8; 
/* see refro.c in slalib, for these suggested values */

slaRefro(zobs,hm,tdk,pmb,rh,wl,phi,tlr,eps,&ref);
refractiond = ref*180.*3600./M_PI;

if((refractiond>=0.)&&(refractiond<=3000.)){
*el = *el + ref;
*refraction = (float)refractiond;
} else { printf("Refraction error. Check weather parameters.\n");}

}
