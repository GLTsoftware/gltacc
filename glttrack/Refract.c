#include <math.h>
#include <stdio.h>

#define REFLOWLIM 0.0
#define REFHIGHLIM 3000.0

void Refract (double *el, int radio_flag, double temp, double humid, double pres,float *refraction)
{


	double z;
	double E[13],nest,y,ord;
	double fptem, fradio, optical, radio;
	int j,k;
	
	double  dpi =   3.141592653589793;      
	

	
	z = (dpi / 2. - *el) * 180. / dpi;

/*
	printf("Elevation(refract.c)=%lf\n",*el);
	printf("radioflag(refract.c)=%d\n",radio_flag);
	printf("temp(refract.c)=%lf\n",temp);
	printf("humid(refract.c)=%lf\n",humid);
	printf("pres(refract.c)=%lf\n",pres);
*/
	
/* define constants in array E (these are polynomial coefficients) */

		E[0] = 0.0;
		E[1] = 46.625;
		E[2] = 45.375;
		E[3] = 4.1572;
		E[4] = 1.4468;
		E[5] = 0.25391;
		E[6] = 2.2716;
		E[7] = -1.3465;
		E[8] = -4.3877;
		E[9] = 3.1484;
		E[10] = 4.520;
		E[11] = -1.8982;
		E[12] = 0.8900;
		

/*
	pres=1000.;
	temp=10.;
	humid=25.;
	
*/
	


		
		temp += 273.;

/*        printf("temp(refract.c)=%lf\n",temp);*/
/* calculating some coefficients */

	fptem = ( pres / 1000.) * (273. / temp);

    /*    printf("Value of fptem(refract.c)=%lf\n",fptem);*/

      y = exp ( (( (temp) * 17.149) - 4684.1) / ((temp) - 38.45));
/*	y = exp(1.0);
*/

  /*      printf("Value of y(refract.c)=%lf\n",y);*/

	fradio = 1.0 + (( y * (humid) * 220.) / (temp * (pres) * 0.760)) ;
/*
	printf("Value of fradio(refract.c)=%lf\n",fradio);
*/
		
			E[0] = ( z - E[1] ) / E[2]  ;

			nest = E[11];
			
/* evaluating the polynomial    */

			for(j=1; j<=8 ; j++)
			{
			k=11-j;

			nest = nest * E[0] + E[k];
			}

			if ( nest <= -80.) nest = 0.;
			
			nest = exp(nest) - E[12] ;

/* chosing optical or radio correction according to radio_flag */

		if (radio_flag == 0)  {
			optical = nest * fptem ;
			*refraction=optical;
		if((*refraction<REFLOWLIM)||(*refraction>REFHIGHLIM)){
		*refraction=0.0;
		fprintf(stderr,"Refraction correction value out-of-range.\n");
		}
			optical = optical / 3600.;
			z = z - optical;
			}
			
		
			
		if  (radio_flag == 1)  {                        
		 radio = nest * fptem * fradio ;
		 *refraction=radio;
		if((*refraction<REFLOWLIM)||(*refraction>REFHIGHLIM)){
		*refraction=0.0;
		fprintf(stderr,"Refraction correction value out-of-range.\n");
		}
		 radio = radio/3600.;
		 z = z - radio;
		 }
		 
		
		z=z*dpi/180.;


		
		*el = dpi / 2.0 - z ;
/*
	printf("The elevation returning from refract.c = %lf\n",*el);
*/

}
