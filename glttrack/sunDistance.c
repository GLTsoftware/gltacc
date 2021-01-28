#include <stdio.h>
#include <math.h>
#include <stdlib.h>

double sunDistance(double az1,double el1,double az2,double el2);

int main(int argc, char *argv[]) {
        double pi, radian,sund;
        double az1,az2,el1,el2;
        
        pi = 4.0 * atan(1.0);
        radian = pi / 180.;
        az1=atof(argv[1]);
        el1=atof(argv[2]);
        az2=atof(argv[3]);
        el2=atof(argv[4]);

        sund=sunDistance(az1,el1,az2,el2);
        sund/=radian;
        printf("%f\n",sund);
return 0;
}


double sunDistance(double az1,double el1,double az2,double el2)
{
double cosd,sind,d;

cosd=sin(el1)*sin(el2)+cos(el1)*cos(el2)*cos(az1-az2);
sind=pow((1.0-cosd*cosd),0.5);
d=atan2(sind,cosd);
return d;
}
