#include <math.h>
#include <curses.h>
#include<string.h>
#include "track.h"

void hms(double *fx, int *fh, int *fm, double *fs,short *dec_sign);
void af(int *i,char s[2]);
void initialize();

void screen(char *source,double *lst_disp,double *utc_disp,double *tjd_disp,
	double *ra_disp,double *dec_disp, 
	double *ra_cat_disp, double *dec_cat_disp,
	double *az_disp,
	double *el_disp,int *icount,double *azoff,double *eloff,int *icflag,
	double *az_actual_corrected,double *el_actual_disp,double *tiltx, double *tilty,int *initflag,int *pvt,float *pressure, float *temperature, float *humidity,
	double *az_error, double *el_error,double *scan_unit, char *messg,
	unsigned short *track_flag, unsigned short *slew_flag,double *focus_counts, double *subx_counts, double *suby_counts,double *subtilt_counts,
double *subtilt_arcseconds,
	double *total_power_disp,double *syncdet_disp, int *integration,
		float *magnitude, char *sptype, float *windspeed, 
	float *winddirection,float *refraction, float *pmdaz, float *pmdel,
	double *smoothed_tracking_error, double *tsys, short *errorflag, short *waitflag, unsigned short *pmac_command_flag, int *antennaNumber, int
*radio_flag,int *padid,double *planetdistance,double *Az_cmd,
double *El_cmd, double *Az_cmd_rate,double *El_cmd_rate,int *milliseconds,
int *servomilliseconds,char *m3StateString)
{

int i,j,l;
short az_act_sign,az_sign,el_sign,dec_dum_sign,dec_app_sign,dec_cat_sign;
char k;
char str[2];
int lsth,lstm,ra_cat_h,ra_cat_m,dec_cat_d,dec_cat_m;
int dec_app_d,dec_app_m;
int ra_app_h,ra_app_m;
int utch,utcm;
int epochint;
int az_cmd_d,az_cmd_m,el_cmd_d,el_cmd_m;
int az_act_d,az_act_m,el_act_d,el_act_m;
double ra_cat_s,dec_cat_s,ra_app_s,dec_app_s,lsts,utcs;
double az_cmd_s,el_cmd_s,az_act_s,el_act_s;
int lstsi,utcsi,az_cmd_si,az_act_si,el_cmd_si,el_act_si;
double ha;


	ha=*lst_disp-*ra_disp;

if((*icount%30)==0) initialize();

	if(*icount==1) initialize();
/*
	if((*icount%2)!=0) return;
*/

	*az_disp=*az_disp/0.017453293;
	*el_disp=*el_disp/0.017453293;


	hms(ra_disp,&ra_app_h,&ra_app_m,&ra_app_s,&dec_dum_sign);
	hms(dec_disp,&dec_app_d,&dec_app_m,&dec_app_s,&dec_app_sign);
	hms(ra_cat_disp,&ra_cat_h,&ra_cat_m,&ra_cat_s,&dec_dum_sign);
	hms(dec_cat_disp,&dec_cat_d,&dec_cat_m,&dec_cat_s,&dec_cat_sign);
	hms(lst_disp,&lsth,&lstm,&lsts,&dec_dum_sign);
	hms(utc_disp,&utch,&utcm,&utcs,&dec_dum_sign);
	hms(az_disp,&az_cmd_d,&az_cmd_m,&az_cmd_s,&az_sign);
	hms(el_disp,&el_cmd_d,&el_cmd_m,&el_cmd_s,&el_sign);
	hms(az_actual_corrected,&az_act_d,&az_act_m,&az_act_s,&az_act_sign);
	hms(el_actual_disp,&el_act_d,&el_act_m,&el_act_s,&dec_dum_sign);



	lstsi=(int)lsts;
	utcsi=(int)utcs;
	az_cmd_si=(int)az_cmd_s;
	el_cmd_si=(int)el_cmd_s;
	az_act_si=(int)az_act_s;
	el_act_si=(int)el_act_s;
	
box(stdscr, '|','-');

move(1,59);
printw("M3:%7s",m3StateString);

/*
move(1,2);
printw("%f %f %f %f %d %d %f",*Az_cmd/3600000.,*El_cmd/3600000.,*Az_cmd_rate/3600000.,*El_cmd_rate/3600000.,*milliseconds,*servomilliseconds,*utc_disp);

move(2,2);
printw("%f %f %f",*az_disp,*el_disp,*utc_disp);
*/
move(2,2);
if(*antennaNumber==1) addstr("SMA Antenna-1    tracking   ");
if(*antennaNumber==2) addstr("SMA Antenna-2    tracking   ");
if(*antennaNumber==3) addstr("SMA Antenna-3    tracking   ");
if(*antennaNumber==4) addstr("SMA Antenna-4    tracking   ");
if(*antennaNumber==5) addstr("SMA Antenna-5    tracking   ");
if(*antennaNumber==6) addstr("SMA Antenna-6    tracking   ");
if(*antennaNumber==7) addstr("SMA Antenna-7    tracking   ");
if(*antennaNumber==8) addstr("SMA Antenna-8    tracking   ");
move(3,2);
printw("on pad: %d",*padid);
move(4,5);
addstr("LST");
move (4,15);
addstr("UTC");
move(4,25);
addstr("TJD");
/*
move(2,25);
addstr("SOURCE=");
*/
move(2,32);
addstr(source);
move(3,32);
addstr(sptype);
move(4,32);
printw("%3.1f",*magnitude);
move (5,3);
af(&lsth,str);
addch(str[0]);
addch(str[1]);
move(5,6);
af(&lstm,str);
addch(str[0]);
addch(str[1]);
move(5,9);
af(&lstsi,str);
addch(str[0]);
addch(str[1]);
move(5,13);
af(&utch,str);
addch(str[0]);
addch(str[1]);
move(5,16);
af(&utcm,str);
addch(str[0]);
addch(str[1]);
move(5,19);
af(&utcsi,str);
addch(str[0]);
addch(str[1]);
move(5,23);
printw("%lf",*tjd_disp);
move(6,3);
printw("%.4f",ha);
move(7,14);
addstr("RA");
move(7,30);
addstr("DEC");
move(9,2);
addstr("CATALOG");
move(9,12);
af(&ra_cat_h,str);
addch(str[0]);
addch(str[1]);
move(9,15);
af(&ra_cat_m,str);
addch(str[0]);
addch(str[1]);
move(9,18);
printw("%.3f",ra_cat_s);
move(9,27);
if (dec_cat_sign>0) addch('+');
if (dec_cat_sign<0) addch('-');
if (dec_cat_sign==0) addch(' ');
af(&dec_cat_d,str);
addch(str[0]);
addch(str[1]);
move(9,31);
af(&dec_cat_m,str);
addch(str[0]);
addch(str[1]);
move(9,34);
printw("%.2f",dec_cat_s);

move(11,2);
printw("APPARENT");
move(11,12);
af(&ra_app_h,str);
addch(str[0]);
addch(str[1]);
move(11,15);
af(&ra_app_m,str);
addch(str[0]);
addch(str[1]);
move(11,18);
printw("%.3f",ra_app_s);
move(11,27);
if(dec_app_sign>0)addch('+');
if(dec_app_sign<0)addch('-');
if(dec_app_sign==0)addch(' ');
af(&dec_app_d,str);
addch(str[0]);
addch(str[1]);
move(11,31);
af(&dec_app_m,str);
addch(str[0]);
addch(str[1]);
move(11,34);
printw("%.2f",dec_app_s);

move(13,14);
addstr("AZ");
move(13,30);
addstr("EL");

move(15,2);
addstr("CMD");
move(15,9);
if((az_sign<0)&&(az_cmd_d==0))addch('-');
move(15,10);
printw("%03d",az_cmd_d);
move(15,15);
af(&az_cmd_m,str);
addch(str[0]);
addch(str[1]);
move(15,18);
af(&az_cmd_si,str);
addch(str[0]);
addch(str[1]);
move(15,27);
if(el_sign>=0)addch(' ');
if(el_sign<0)addch('-');
move(15,28);
af(&el_cmd_d,str);
addch(str[0]);
addch(str[1]);
move(15,31);
af(&el_cmd_m,str);
addch(str[0]);
addch(str[1]);
move(15,34);
af(&el_cmd_si,str);
addch(str[0]);
addch(str[1]);
move(17,2);
addstr("ACTUAL");
move(17,9);
if((az_act_sign<0)&&(az_act_d==0))addch('-');
move(17,10);
printw("%03d",az_act_d);
move(17,15);
af(&az_act_m,str);
addch(str[0]);
addch(str[1]);
move(17,18);
af(&az_act_si,str);
addch(str[0]);
addch(str[1]);
move(17,28);
af(&el_act_d,str);
addch(str[0]);
addch(str[1]);
move(17,31);
af(&el_act_m,str);
addch(str[0]);
addch(str[1]);
move(17,34);
af(&el_act_si,str);
addch(str[0]);
addch(str[1]);
move(19,2);
addstr("ERROR");
move(19,11);
printw("%7.1f\"",(*az_error*3600.));
move(19,28);
printw("%7.1f\"",(*el_error*3600.));
/*move(18,23);
printw("(%.1f\")",*smoothed_tracking_error);
*/

move(20,12);
printw("(%6.0f)",*pmdaz);
move(20,32);
printw("(%6.0f)",*pmdel);
move(21,2);
addstr("OFFSETS(\")");
move(21,12);
printw("%6.0f",*azoff);
move(21,32);
printw("%6.0f",*eloff);
move(22,2);
printw("%6.0f",*scan_unit);
move(21,42);
addstr("TILTS");
move(21,50);
printw("%.1f",*tiltx);
move(21,63);
printw("%.1f",*tilty);
move(21,73);
printw("%5d",*integration);

/*

move(22,42);
printw("%3d",*errorflag);
move(22,50);
printw("%3d",*waitflag);
move(22,55);
printw("%3d",*pmac_command_flag);
*/
move(22,42);
printw("Planet dist.=%.6f AU",*planetdistance);

/*
move(22,42);
addstr("PVT");
move(22,46);
printw("%4d",*pvt);
move(22,52);
addstr("TF=");
printw("%1d",*track_flag);
move(22,57);
addstr("SF=");
printw("%1d",*slew_flag);
*/
move(18,42);
addstr("REFRACTION:");
move(18,53);
if(*radio_flag==1) printw("%5.1f \" (radio)   ",*refraction);
if(*radio_flag==0) printw("%5.1f \" (optical) ",*refraction);
move(19,42);
addstr("WEATHER:");
move(19,51);
printw("%4.1f C",*temperature);
move(19,59);
printw("%5.1f%%",*humidity);
move(19,67);
printw("%6.1f mbar",*pressure);
move(20,51);
printw("wind: %4.1f m/s,  %5.1f  deg",*windspeed,*winddirection);
move(2,42);
addstr(messg);
move(17,42);
addstr("SUB_Z:");
move(17,49);
printw("%8.0f cts, %4.2f mm ",*focus_counts, (*focus_counts)/2000.);

move(13,42);
addstr("SUB_X:");
move(13,49);
printw("%8.0f cts, %4.2f mm ",*subx_counts, (*subx_counts)/1000.);

move(15,42);
addstr("SUB_Y:");
move(15,49);
printw("%8.0f cts, %4.2f mm ",*suby_counts, (*suby_counts)/500.);

move(11,42);
addstr("SUB_TILT:");
move(11,52);
printw("%8.0f cts, %4.2f \" ",*subtilt_counts, *subtilt_arcseconds);

move(9,42);
addstr("CONT. DET. O/P:");
move(9,58);
printw("%8.2f mV", (*total_power_disp)*1000.);

move(7,42);
addstr("SYNC. DET. O/P:");
move(7,58);
printw("%8.2f mV", (*syncdet_disp)*1000.);

move(5,42);
addstr("TSYS:");
move(5,48);
printw("%05.0f", *tsys);
move(5,55);
addstr("K");


/*
for(l=1;l<23;l++)
{
move(l,40);
addstr("|");
}
*/
refresh();
}

void hms(double *fx, int *fh, int *fm, double *fs,short *dec_sign)
{
	double fmt;
	double absfx;

	if(*fx<0.) {
		absfx=-*fx;
		*dec_sign=-1;
		}
	if(*fx>=0.) {
		absfx=*fx;
		*dec_sign=1;
		}
    *fh = (int)absfx;
    fmt = (absfx - *fh) * 60.;
    *fm = (int) fmt;
    *fs = (fmt - *fm) * 60.;
    if (*fx < 0.) {
	*fh = -(*fh);
    }
}

void af(int *i,char s[2])
{
int j,k,l;
if(*i<0)*i=-*i;
j=48+*i;
if(*i<10) {s[0]='0';s[1]=(char)j;}
if(*i>=10){
k=*i%10;
l=(*i-k)/10;
j=48+k;
s[1]=j;
j=48+l;
s[0]=j;
}
}

void initialize()
{
initscr();
box(stdscr, '|','-');
move(2,2);
/*
if(*antennaNumber==1) addstr("SMA Antenna-1    tracking   ");
if(*antennaNumber==2) addstr("SMA Antenna-2    tracking   ");
if(*antennaNumber==3) addstr("SMA Antenna-3    tracking   ");
addstr("SMA Antenna-2");
*/
refresh();
}
