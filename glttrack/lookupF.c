#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <smapopt.h>
#include "novas.h"
#include "vme_sg_simple.h"
#include "commonLib.h"
#include "SpiceUsr.h"
#define UTCLEN 34

#define MINMAG 2.
#define MAXMAG 9.

#define PAD1
#include "track.h"

#define T1950   2433281.0
#define T2000   2451544.0

#define MSEC_PER_DEG 3600000.
#define MSEC_PER_HR 3600000.      
#define AUPERDAY_KMPERSEC 1731.45717592 


struct source
{
    char            sourcename[20], veltype[20], comment[100], decsign;
    int             rah, ram, decd, decm;
    float           ras, decs, epoch, vel, pmr, pmd;
};

/************************ Function declarations *************************/
double tjd2et(double tjd);
void   azelcal(double *lst, double *ra, double *dec, double *az, double *el);
void            print_upper(char *name);
void            pad(char *s, int length);
void            is_planet(char *s, int *flag, int *id);
void            starcat(char *s,int *star_flag,struct source * observe_source,int optical);
double sunDistance(double az1,double el1,double az2,double el2);
void usage(char *error, char *addl);
void getNearbySources(double lst, double az, double el, double width,int
optical);

/** Global variables *********************************************** */

int	first_time_spk=1; /* this variable is used for opening the 
		ephemeris file only once, on  first pass */

        smapoptContext optCon;
double  pi ;
double 	radian;
double LONGITUDE_HOURS = -10.365168199815;
        double LATITUDE_DEGREES =   19.824205263889;
        double LONGITUDE_DEGREES =   -155.477522997222;
        double SINLAT =   0.339135375955;
        double COSLAT =   0.940737581250;
        double LONGRAD =   -2.713594689147;
        double HEIGHT_M =   4083.948144000000;



/*end of global variables***************************************************/

main(int argc, char *argv[])
{

    double          ra, dec, lst,  lst_radian ;
	double raApp,decApp;
	double	sun_distance;
    double          ra_cat, dec_cat ;
    double 		radialVelocity;
    float           epoch=2000.;
    double          tjd ;
    double          az, el;
    double          hr;
    int             nplanet;
	double	epochd=2000.,velocity=0.0;
	int	newSourceFlag=0;
    /* The following variables are defined for time calculations */
 int              hours, minutes;	
	double seconds,delta=0.;

            SpiceDouble      et;
            SpiceChar        utcstr[UTCLEN];
            SpiceChar        dutcstr[UTCLEN];
            SpiceChar        jutcstr[UTCLEN];
	    SpiceChar        inputTimeString[50];
	    char	     junk[3];

    /* Variables for LST calculation */
    double          d1, dtupi, gst;

    /* end of time variables definitions */

    /* the following variables are from name.c */
    char            sname[34], sname2[34];
    int             sol_sys_flag, id, star_flag;
    struct source   observe_source;
	double sunaz,sunel;
	int device_fd,irig_status;
	struct vme_sg_simple_time ts;

/* for earthtilt and sidereal_time */
	double equinoxes,tjd_upper,tjd_lower,dpsi,deps,tobl,mobl;
	
	body Planet;
	body earth = {0, 399, "earth"};
        site_info location = {LATITUDE_DEGREES,LONGITUDE_DEGREES,HEIGHT_M,0.0,0.0};       
	cat_entry star = {"cat","star",0,0.,0.,0.,0.,0.,0.};
	double distance=0.;

        struct tm ;
	double pos1950[3],pos2000[3];
	double museconds;

	char c, *source,*timeString;
	int gotsource=0,gotra=0,gotdec=0,gotepoch=0;
	int gotapparent=0;
	int gottime=0,gotcoordinates=0;
	int gotoptical=0;
	int gotn=0;
	double width;
        char smapoptc;

	  struct  smapoptOption optionsTable[] = {
                {"source",'s',SMAPOPT_ARG_STRING,&source,'s', "source name."},
                {"ra",'r',SMAPOPT_ARG_TIME,&ra,'r',
                " RA in hours  (decimal or HH:MM:SS.SSS format"},
                {"dec",'d',SMAPOPT_ARG_DEC,&dec,'d',
                "DEC in degrees (decimal or +-DD:MM:SS.SSS"},
                {"epoch",'e',SMAPOPT_ARG_DOUBLE,&epochd,'e',
                "Epoch of input coordinates 1950 or 2000."},
                {"vel",'v',SMAPOPT_ARG_DOUBLE,&velocity,'v',
                "Velocity in km/s (optional)"},
		{"time",'t',SMAPOPT_ARG_STRING,&timeString,'t',
			"date time: DD MonthName YYYY HH:MM:SS."},
                {"nearby",'n',SMAPOPT_ARG_DOUBLE,&width,'n',
                "Get sources nearby, within box fullwidth in deg."},
                {"apparent",'a',SMAPOPT_ARG_NONE,0,'a',
                "To request also for apparent ra and dec"},
                {"optical",'o',SMAPOPT_ARG_NONE,0,'o',
                "Lookup optical catalog instead of readio catalog."},
                SMAPOPT_AUTOHELP
                {NULL,0,0,NULL,0},
                "Command to lookup a source's coordinates. 
			--help gives a detailed usage.\n"
        };
	
    /* END OF VARIABLE DECLARATIONS */

    /********Initializations**************************/

    pi = 4.0 * atan(1.0);
    dtupi = pi * 2.0;
    radian = pi / 180.;

 if(argc<2) usage("Insufficient number of arguments","At least
source- name required.");
        optCon = smapoptGetContext("observe", argc, argv,
optionsTable,0);


  while ((c = smapoptGetNextOpt(optCon)) >= 0) {
        switch(c) {
                case 'h':
                usage(NULL,NULL);
                break;

                case 's':
                gotsource=1;
                break;

                case 't':
                gottime=1;
                break;

                case 'r':
                gotra=1;
                break;

                case 'd':
       		gotdec=1;
                break;

                case 'e':
                gotepoch=1;
                break;
	
		case 'n':
		gotn=1;
		break;

		case 'o':
		gotoptical=1;
		break;

		case 'a':
		gotapparent=1;
		break;

                }

        }

        if(gotsource!=1) usage("No source specified","Source name is
required
 .\n");

  if((gotra==1)||(gotdec==1)) {
                if(!((gotra==1)&&(gotdec==1)&&(gotsource==1))) {
                usage("Insufficient arguments.",
        "if coordinates are specified, source-name, ra and dec are required;
        epoch and velocity is optional.\n");
                }
                else  {
                if(gotepoch==0) epoch=2000.;
		if(gotepoch==1) epoch=(float)epochd;
		newSourceFlag=1;
                }
        }

  if(c<-1) {
        fprintf(stderr, "%s: %s\n",
                smapoptBadOption(optCon, SMAPOPT_BADOPTION_NOALIAS),
                smapoptStrerror(c));
        }
         smapoptFreeContext(optCon);

	if(!gottime) {
	device_fd = open("/dev/vme_sg_simple", O_RDWR);     
	if(device_fd==-1) { 
		perror("open()");
		fprintf(stderr,"Could not open vme_sg_simple device");
		exit(-1);
	}
	}

    /********************end of initializations*******************/

	strcpy(sname,source);

    print_upper(sname);

	strcpy(sname2,sname);
	pad(sname, 20);

    if(newSourceFlag==0) {
	
	 is_planet(sname, &sol_sys_flag, &id);

	if (sol_sys_flag == 1)
	{
	  
	    nplanet = id;
	
		/* in the new ephemeris codes, moon is 10, not 301 */
		/*
		if(nplanet==301) nplanet=10;
		commented out on 11 jan 2001, now we are back to
		ansi-C codes from jpl and not using hoffman's package
		to read the jpl ephemeris files*/
	   Planet.type=0;
	   Planet.number=nplanet;
	  strcpy(Planet.name,sname);
	}
	if (sol_sys_flag == 0)
	{
            starcat(sname, &star_flag, &observe_source,gotoptical);

	    if (star_flag == 0)
	    {
		nplanet = 0;
	/* unknown source. go into target mode with current
	position as commanded position */

		fprintf(stderr,"%s\n","Source not found");
		exit(0);
	    }

	    if (star_flag == 1)
	    {
		nplanet = 0;
		dec_cat = fabs(observe_source.decd) + observe_source.decm / 60. + observe_source.decs / 3600.;
		ra_cat = observe_source.rah + observe_source.ram / 60. + observe_source.ras / 3600.;
		if (observe_source.decsign == '-') dec_cat = -dec_cat;

/* if the coordinates are B1950, precess them to J2000 first*/
		epoch = observe_source.epoch;

		if (epoch == 1950.)
		{
			radec2vector(ra_cat,dec_cat,1.0,pos1950);
                        precession(T1950,pos1950,T2000,pos2000);
                        vector2radec(pos2000,&ra_cat,&dec_cat);

		}
		star.ra=ra_cat;
		star.dec=dec_cat;
		/* converting input pm-ra in mas/yr to sec/century */
		star.promora=
		 (double)observe_source.pmr/15./cos(dec_cat*radian)/10.;
		star.promodec=(double)observe_source.pmd/10.;
	    }			/* star_flag if */
	}			/* sol_sys_flag if */

     } /* if newSourceFlag==0*/

     if(newSourceFlag==1) { 
	ra_cat=ra;
	dec_cat=dec;
	observe_source.pmr=0.0;
	observe_source.pmd=0.0;
		if (epoch == 1950.) {
			radec2vector(ra_cat,dec_cat,1.0,pos1950);
                        precession(T1950,pos1950,T2000,pos2000);
                        vector2radec(pos2000,&ra_cat,&dec_cat);
		}
		star.ra=ra_cat;
		star.dec=dec_cat;
		/* converting input pm-ra in mas/yr to sec/century */
		star.promora=
		 (double)observe_source.pmr/15./cos(dec_cat*radian)/10.;
		star.promodec=(double)observe_source.pmd/10.;
		star_flag=1;
		sol_sys_flag=0;
		nplanet=0;
     } /* if newSourceFlag==1*/


	if(!gottime) {

	    irig_status = read(device_fd,&ts,1);
            if(irig_status==-1) {
                perror("read()");
                exit(-1);
            }
	    close(device_fd);
 
	    hours = ts.hour;
	    minutes = ts.min;
	    seconds = (double) ts.sec;
	    museconds = (double) ts.usec;
	    seconds += (museconds/1.0e6);

	hr = (double)hours + ((double)minutes)/60. + seconds/3600.;
	/* calculating julian day */
	   tjd_upper = (double)((int)(365.25*(ts.year+4711))) + 
			(double)ts.yday + 351.5;
	   tjd_lower=hr/24.;
	   tjd=tjd_upper+tjd_lower;

		} else {
 
            /* Load the kernel pool.*/
 
            ldpool_c ( "/global/catalogs/time.ker");
 
            /* Compute result for each new time epoch. */
 
		strcpy(inputTimeString,timeString);
 
               str2et_c ( inputTimeString, &et );
 
               /* Convert from et to string */
 
               et2utc_c (  et , "J", 8, UTCLEN, jutcstr );
		sscanf(jutcstr,"%s %lf",junk,&tjd);
		tjd_lower=modf(tjd,&tjd_upper);
		tjd_lower=tjd-tjd_upper;
 
           }
	
	    delta=32.184+LEAPSECONDS;

	 /* obtain equation of equinoxes which is needed for LST */
	earthtilt(tjd_upper,&mobl, &tobl, &equinoxes,&dpsi, &deps);
	sidereal_time(tjd_upper, tjd_lower,equinoxes, &gst);
	d1 = gst *dtupi / 24.+LONGRAD;
	lst_radian = fmod(d1, dtupi);
	if (lst_radian < 0.) {
	    lst_radian += dtupi;
	}
	/* converting lst to hours */
	lst = lst_radian * 24. / dtupi;

	if (nplanet != 0)
	{
	   topo_planet(tjd, &Planet,&earth, delta, 
		&location, &ra, &dec, &distance,&radialVelocity);
		radialVelocity *= AUPERDAY_KMPERSEC;
	    raApp=ra;
	    decApp=dec;
	}
	if (nplanet == 0) {
	    topo_star(tjd,&earth,delta, &star,&location, &ra, &dec);
	    raApp=ra;
	    decApp=dec;
	}

	    ra = ra * 15.0 * pi / 180.;
	    dec = dec * pi / 180.;
	    azelcal(&lst_radian, &ra, &dec, &az, &el);

	   Planet.type=0;
	   Planet.number=10;
	  strcpy(Planet.name,"sun");
	   topo_planet(tjd, &Planet,&earth, delta, 
		&location, &ra, &dec, &distance,&radialVelocity);
		radialVelocity *= AUPERDAY_KMPERSEC;
	    ra = ra * 15.0 * pi / 180.;
	    dec = dec * pi / 180.;
	    azelcal(&lst_radian, &ra, &dec, &sunaz, &sunel);
	    sun_distance= sunDistance(az,el,sunaz,sunel);
	az=az/radian;
	el=el/radian;

if(gotapparent) printf("%f %f %f %f %f \n",az,el, sun_distance,raApp,decApp);
else printf("%f %f %f\n",az,el, sun_distance);

if(gotn==1) {
getNearbySources(lst_radian,az,el,width,0);
getNearbySources(lst_radian,az,el,width,1);
}

return 0;
}

/*------------name.c--------*/
void
print_upper(char *name)
{
    register int    t;

    for (t = 0; name[t]; ++t)
    {
	name[t] = tolower(name[t]);

    }

}

void
pad(char *s, int length)
{
    int             l;

    l = strlen(s);
    while (l < length)
    {
	s[l] = ' ';
	l++;
    }

    s[l] = '\0';
}


/*
 * This table is to be extended as and when we acquire more ephemeris data
 * from JPL for other minor bodies
 */

void
is_planet(char *s, int *flag, int *id)
{
    if (!strcmp(s, "mercury             "))
    {
	*id = 1;
	*flag = 1;
    } else if (!strcmp(s, "venus               "))
    {
	*id = 2;
	*flag = 1;
    } else if (!strcmp(s, "earth               "))
    {
	*id = 3;
	*flag = 1;
    } else if (!strcmp(s, "mars                "))
    {
	*id = 499;
	*flag = 1;
    } else if (!strcmp(s, "jupiter             "))
    {
	*id = 5;
	*flag = 1;
    } else if (!strcmp(s, "saturn              "))
    {
	*id = 6;
	*flag = 1;
    } else if (!strcmp(s, "uranus              "))
    {
	*id = 7;
	*flag = 1;
    } else if (!strcmp(s, "neptune             "))
    {
	*id = 8;
	*flag = 1;
    } else if (!strcmp(s, "pluto               "))
    {
	*id = 9;
	*flag = 1;
    } else if (!strcmp(s, "moon                "))
    {
	*id = 301;
	*flag = 1;
    } else if (!strcmp(s, "sun                 "))
    {
	*id = 10;
	*flag = 1;
    } else if (!strcmp(s, "titan               "))
    {
	*id = 606;
	*flag = 1;
    } else if (!strcmp(s, "io                  "))
    {
	*id = 501;
	*flag = 1;
    } else if (!strcmp(s, "europa              "))
    {
	*id = 502;
	*flag = 1;
    } else if (!strcmp(s, "callisto            "))
    {
	*id = 504;
	*flag = 1;
    } else if (!strcmp(s, "ganymede            "))
    {
	*id = 503;
	*flag = 1;
    } else if (!strcmp(s, "mimas               "))
    {
	*id = 601;
	*flag = 1;
    } else if (!strcmp(s, "enceladus           "))
    {
	*id = 602;
	*flag = 1;
    } else if (!strcmp(s, "tethys              "))
    {
	*id = 603;
	*flag = 1;
    } else if (!strcmp(s, "dione               "))
    {
	*id = 604;
	*flag = 1;
    } else if (!strcmp(s, "rhea                "))
    {
	*id = 605;
	*flag = 1;
    } else if (!strcmp(s, "hyperion            "))
    {
	*id = 607;
	*flag = 1;
    } else if (!strcmp(s, "iapetus             "))
    {
	*id = 608;
	*flag = 1;
    } else if (!strcmp(s, "triton              "))
    {
	*id = 801;
	*flag = 1;
    } else if (!strcmp(s, "nereid              "))
    {
	*id = 802;
	*flag = 1;
    } else if (!strcmp(s, "ceres               "))
    {
        *id = 375;
        *flag = 1;
    } else if (!strcmp(s, "pallas              "))
    {
        *id = 376;
        *flag = 1;
    } else if (!strcmp(s, "hygiea              "))
    {
        *id = 377;
        *flag = 1;
    } else if (!strcmp(s, "vesta               "))
    {
	*id = 378;
	*flag = 1;
    } else if (!strcmp(s, "c2001q4neat         "))
    {
        *id = 379;
        *flag = 1;
    } else if (!strcmp(s, "c2002t7linear       "))
    {
        *id = 380;
        *flag = 1;
    } else if (!strcmp(s, "c2002t7linear       "))
    {
        *id = 380;
        *flag = 1;
    } else if (!strcmp(s, "c2000wm1linear      "))
    {
        *id = 381;
        *flag = 1;
    } else if (!strcmp(s, "c2004q2             "))
    {
        *id = 382;
        *flag = 1;
    } else if (!strcmp(s, "tempel1             "))
    {
        *id = 383;
        *flag = 1;
    } else
    {
	*id = 0;
	*flag = 0;
    }
}

/* The following function does the standard star catalog look up */

void
starcat(char *s, int *star_flag, struct source * observe_source,int optical)
{

    FILE           *fp2;
    int             end_of_file;
    int             number;
    int             rah, ram, decd, decm;
    float            vel,epoch, ras, decs, pmr, pmd;
    char            sptype[20],vtype[20], source_name[20], comment[100];
    char            decsign;

    *star_flag = 0;

	if(optical==0) {
	fp2 = fopen("/global/catalogs/sma_catalog", "r");
        end_of_file = fscanf(fp2, 
	"%s %d %d %f %c%2d %d %f %f %f %f %s %f %s",
	source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
		    &pmr, &pmd, &epoch, vtype, &vel, comment);
	}
	
	if(optical==1) {
	fp2 = fopen("/global/catalogs/sma_optical_catalog", "r");
        end_of_file = fscanf(fp2, 
	"%d %s %d %d %f %c%2d %d %f %f %f %f %s %f %s %s",
	&number,source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
		    &pmr, &pmd, &epoch, vtype, &vel, comment,sptype);
	}

        while (end_of_file !=EOF) {

        if(optical==0) {
        end_of_file = fscanf(fp2,
        "%s %d %d %f %c%2d %d %f %f %f %f %s %f %s",
         source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
                    &pmr, &pmd, &epoch, vtype, &vel, comment);
        }

        if(optical==1) {
        end_of_file = fscanf(fp2,
        "%d %s %d %d %f %c%2d %d %f %f %f %f %s %f %s %s",
         &number,source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
                    &pmr, &pmd, &epoch, vtype, &vel, comment, sptype);
        }

	pad(source_name, 20);
	print_upper(source_name);

	if (!strcmp(s, source_name))
	{
	    *star_flag = 1;
	strcpy(observe_source->sourcename, source_name);
	observe_source->rah = rah;
	observe_source->ram = ram;
	observe_source->ras = ras;
	observe_source->decsign = decsign;
	observe_source->decd = decd;
	observe_source->decm = decm;
	observe_source->decs = decs;
	observe_source->pmr = pmr;
	observe_source->pmd = pmd;
	observe_source->epoch = epoch;
	strcpy(observe_source->veltype, vtype);
	observe_source->vel = 0.0;
	strcpy(observe_source->comment, comment);
	break;
	}
        if(optical==0) {
        if (end_of_file!=14) {
        printf("Source catalog is corrupted.\n");
        break;
         }}
        if(optical==1) {
        if (end_of_file!=16) {
        printf("Source catalog is corrupted.\n");
        break;
         }}

        end_of_file=0;
    }
    fclose(fp2);
}

double sunDistance(double az1,double el1,double az2,double el2)
{
double cosd,sind,d;

cosd=sin(el1)*sin(el2)+cos(el1)*cos(el2)*cos(az1-az2);
sind=pow((1.0-cosd*cosd),0.5);
d=atan2(sind,cosd);
d=d/radian;
return d;
}

double tjd2et(double tjd)
{
  return((tjd-2451545.)* 86400.0+32.184+LEAPSECONDS);
}

void usage(char *error, char *addl) {

        if (error) fprintf(stderr, "\n%s: %s\n\n", error, addl);
        smapoptPrintHelp(optCon, stderr, 0);
        fprintf(stderr,
            "\ne.g.: usage:\n"
		"lookup -s jupiter\n"
		"lookup -s jupiter -a\n"
		"lookup -s jupiter -t \"7 Dec 2004 12:23:33\"\n"
		"lookup -s jupiter -n 5 -o\n"
		"-to search for optical stars within 5 deg radius of jupiter.\n"
		);
        exit(1);
}


void getNearbySources(double lst, double az, double el, 
				double width, int optical) {

    FILE           *fp;

	double phi,width_radian,sinphi,cosphi,sintheta,costheta;
	double ra,dec,ra1,ra2,dec1,dec2;
	double catra,catdec;
	double sinh,cosh;
	double hourangle;
	int end_of_file;
	int nsources;
    int             number;
    int             rah, ram, decd, decm;
    float            vel,epoch, ras, decs, pmr, pmd;
    char            sptype[20],vtype[20], source_name[20], comment[100];
    int            decsign;
	float mv;

az=az*radian;
el=el*radian;
width_radian=width*radian/2.;

phi=radian*LATITUDE_DEGREES;
cosphi=cos(phi);
sinphi=sin(phi);

/*calculate declination from elevation and latitude.*/
sintheta=sin(el)*sinphi+cos(el)*cos(az)*cosphi;
costheta=pow((1.0-sintheta*sintheta),0.5);
dec=atan2(sintheta,costheta);

/*calculate hour angle from elevation, azimuth and latitude.*/
sinh=-cos(el)*sin(az);
cosh=sin(el)*cosphi-cos(el)*cos(az)*sinphi;
if (cosh == 0.) {
  hourangle=0.;
} else {
  hourangle=atan2(sinh,cosh);
}

ra=lst-hourangle;

if (ra < 0.) {ra=ra+2.*pi;}
if (ra > 2.*pi) {ra=ra-2.*pi;}

ra1=ra-width_radian/cos(dec);
ra2=ra+width_radian/cos(dec);
dec1=dec-width_radian;
dec2=dec+width_radian;

	if(optical==0) {
	fp = fopen("/global/catalogs/sma_catalog", "r");
    nsources=0;
    while ((end_of_file = fscanf(fp, 
	"%s %d %d %f %d %d %f %f %f %f %s %f %s",
	source_name, &rah, &ram, &ras, &decd, &decm, &decs,
		    &pmr, &pmd, &epoch, vtype, &vel, comment))
	!=EOF)
	{
  catra=rah+ram/60.+ras/3600.;
  catra=catra*radian*15.;
  if(decd<0.){decsign=-1;}
  if(decd>=0.){decsign=1;}
  catdec=abs(decd)+decm/60.+decs/3600.;
  catdec=catdec*radian;
  if(decsign==-1) {catdec=-catdec;}
     if((catra>=ra1)&&(catra<=ra2) && (catdec>=dec1) && (catdec<=dec2))     {
	printf("%s\n",source_name);
      nsources++;
    }
	}
    fclose(fp);
	}

	if(optical==1) {
	fp = fopen("/global/catalogs/hipcat", "r");
    nsources=0;
    while ((end_of_file = fscanf(fp, 
	"%s %d %d %f %d %d %f %f %s",
	source_name, &rah, &ram, &ras, &decd, &decm, &decs, &mv, sptype))
	!=EOF)
	{
  catra=rah+ram/60.+ras/3600.;
  catra=catra*radian*15.;
  if(decd<0.){decsign=-1;}
  if(decd>=0.){decsign=1;}
  catdec=abs(decd)+decm/60.+decs/3600.;
  catdec=catdec*radian;
  if(decsign==-1) {catdec=-catdec;}
     if((catra>=ra1)&&(catra<=ra2) && (catdec>=dec1) && (catdec<=dec2))     {
	printf(" %s %.1f %s\n",source_name,mv,sptype);
      nsources++;
       }
	}
    fclose(fp);
	}

} 
