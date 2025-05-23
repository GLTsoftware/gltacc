/****************************************************************

glttrack.c

Nimesh Patel

version 2.0
22 May 2017
(for Thule operation)
	
This program runs as a daemon on gltacc.
It calculates az, el and rates and communicates these 
to ACU via ethernet interface.
Higher level commands issued from gltobscon are handled via DSM.
Status variables from ACU are copied to DSM for monitoring.

version 2.1
2 Feb 2021
Added Redis writes, to eventually replace DSM.

version 3.0
14 May 2025 NAP
This is from github- 17 Mar 2023 version.
A later version from November 2024 had Two Line mode- but that produced
timing delays and many operations such as scans, pointing, did not work.
In this present version, updates are made to all the ACU communications
program to fix the error of byte order in checksum and data length
variables. These errors were diagnosed on 26 November 2024, and all
the low-level acuCommand C codes were updated.  The same fixes are now
applied here in this version of glttrack.c


version 3.1
16 May 2025 NAP
Implement a Watchdog timer for ACU communications via a new thread,
instead of just opening the connection at the beginning, before entering
the main loop.


*****************************************************************/

#include <stdio.h>
#include <sys/utsname.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <endian.h>
#include <math.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <termio.h>
#include <sys/time.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <pthread.h>
#include <hiredis.h>
#include "smapopt.h"
#include "novas.h"
#include "track.h"
#include "dsm.h"
#include "acu.h"
#include "tsshm.h"
#include "antennaPosition.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

#include "acuData.h"
#include "acuCommands.h"
#include "dsm.h"

#include <poll.h>

#define ACC "gltobscon"

/* redis server (gltobscon) */
#define REDIS_SERVER "192.168.1.11"
#define REDIS_PORT 6379

/* ACU ports, see 3.1.1.2 of ICD */
#define ACU_CONTROL_PORT 9010
#define ACU_MONITOR_PORT 9110

/* ACU IP address */
#define ACU_IP_ADDRESS "192.168.1.103"

#define DEBUG 0
#define COORDINATES_SVC 1
#define T1950   2433281.0
#define T2000   2451544.0

#define MSEC_PER_DEG 3600000.
#define MSEC_PER_HR 3600000.      
#define AUPERDAY_KMPERSEC 1731.45717592 

#define	HIGH_EL_TO_AVOID_SUN_MAS 288000000. 
#define	LOW_EL_TO_AVOID_SUN_MAS 54000000.

#define IDLE_DRIVES_TIMEOUT 300 

#define DSM_HOST "gltobscon"

/* The following limits are according to the values in ACU System Parameters*/
/* as of 7 March 2018 */

#define AZ_CW_LIMIT 360.0
#define AZ_CCW_LIMIT -180.0
#define EL_UP_LIMIT 90.0
#define EL_DOWN_LIMIT 1.89

/* FALSE for testing without servo; no real encoder readings, no antenna motion */
#define SERVO 0

#define NUMPOS 6 /* number of trajectory points for ACU program track mode */

// ACU connection retry logic
#define MAX_CONNECT_RETRIES 10
#define CONNECT_WAIT_SEC 2

/************************************************************************/
struct source
{
    char            sourcename[20], veltype[20], comment[100], decsign;
    int             rah, ram, decd, decm;
    float           ras, decs, epoch, vel, pmr, pmd;
};

/************************ Function declarations *************************/
void handlerForSIGINT(int signum);
void *CommandHandler();
void *ACUstatus();
void *ACUiostatus();
void *ACUmetrology();
void *ACUConnectionWatchdog(void *arg);
int ACUmode(int acuModeCmd);
int ACUAzEl(double cmdAzdeg, double cmdEldeg);
int ACUAzElRate(double cmdAzRate, double cmdElRate);
int ACUprogTrack(int numPos, int timeOfDay[], int azProgTrack[], int elProgTrack[],short clearstack);
short checkSum (char *buff, int size);
short calculate_checksum (char *buff, int size);
void SendMessageToDSM(char *messg);
void SendLastCommandToDSM(char *lastCommand);       
void            local(double *lst, double *ra, double *dec, double *az, double *el, double *tjd, double *azoff, double *eloff, float *pressure, float *temperature, float *humidity, int *radio_flag, float *refraction, float *pmdaz, float *pmdel,  short *target_flag, double *commanded_az,double *commanded_el);

void            split(unsigned long * lw, unsigned short * sw1, unsigned short * sw2);

void            print_upper(char *name);
void            pad(char *s, int length);
void            is_planet(char *s, int *flag, int *id);
void            starcat(char *s, int *star_flag, struct source * observe_source);
void sidtim(double *tjdh,double *tjdl,int *k,double *gst);
double sunDistance(double az1,double el1,double az2,double el2);
double tjd2et(double tjd);

void redisWriteShort(char *hashName, char *fieldName, short variable);
void redisWriteInt(char *hashName, char *fieldName, int variable);
void redisWriteFloat(char *hashName, char *fieldName,float variable);
void redisWriteDouble(char *hashName, char *fieldName, double variable);
void redisWriteString(char *hashName, char *fieldName, char *variable);

/** Global variables *********************************************** */


struct sigaction action, old_action;
int sigactionInt;

pthread_t	CommandHandlerTID, ACUstatusTID,ACUiostatusTID,ACUmetrologyTID,                 watchdogTID;

	struct sched_param param,param2,param3,param4,watchdogParam;
	pthread_attr_t attr,attr2,attr3,attr4,watchdogAttr;
	int policy = SCHED_FIFO;

unsigned long   window;

	int dsm_status;

int	first_time_spk=1; /* this variable is used for opening the 
		ephemeris file only once, on  first pass */

int             user;  /* input command */ 

char            messg[100];
char            lastCommand[100];
short command_flag=0;
char            sptype[10]="----------";
float           magnitude=0.;
int             read_mount_model_flag = 1;
double		azmodelrms,elmodelrms,azdc,azcol,eltilt,aztilt_sin,aztilt_cos,aztilt_sin2,aztilt_cos2,
		azenc_sin,azenc_cos,azenc_sin2,azenc_cos2,
		azenc_sin3,azenc_cos3,
                eldc,elsag,elsagsin,elsagtan,eaztilt_sin,eaztilt_cos,eaztilt_sin2,eaztilt_cos2;
double		razmodelrms,relmodelrms,razdc,razcol,reltilt,raztilt_sin,raztilt_cos,raztilt_sin2,
		raztilt_cos2, reldc,relsag,relsagsin,relsagtan,reaztilt_sin,reaztilt_cos,
		razenc_sin,razenc_cos,razenc_sin2,razenc_cos2,
		razenc_sin3,razenc_cos3, reaztilt_sin2,reaztilt_cos2;
double		pmaztiltAzoff,pmaztiltEloff;
short 		interrupt_command_flag=0;
char pointingParameter[20];
int numParameters=0;
int setTiltFlag,defaultTiltFlag,rdefaultTiltFlag;
char opticalValue[20],radioValue[20];

double 	radian;


#if SERVO
TrackServoSHM *tsshm;    /* Local pointer to shared memory for
                                   communicating az,el and rates to servo */
#endif

double subx_counts=0.,suby_counts=0.,focus_counts=0.,subtilt_counts=0.;
char 		command[159];

int	cal_flag=0;

    int             radio_flag=1;

	double sourceVelocity=0.;

	int receivedSignal=0;

	double polar_dut;
        float polar_dut_f;

	double setFeedOffsetA1=0.,setFeedOffsetA2=0.;


	/* default position read from antennaPosition.h */
	double longitude_hours = LONGITUDE_HOURS;
	double latitude_degrees = LATITUDE_DEGREES;
	double longitude_degrees = LONGITUDE_DEGREES;
	double sinlat = SINLAT;
	double coslat = COSLAT;
	double longrad = LONGRAD;
	double height_m = HEIGHT_M;

	char operatorErrorMessage[500];
       int sockfdControl=0,sockfdMonitor=0;
     
        double az_enc_from_acu,el_enc_from_acu;

char redisData[1024];
redisContext *redisC;
redisReply *redisResp;
struct timeval redisTimeout = {1,500000}; /*1.5 seconds for redis timeout */

/*end of global variables***************************************************/

int main(int argc, char *argv[]) {

    FILE            *fp_mount_model,*fp_polar;

    double          ra, dec, lst, lst_prev, lst_radian, lst_radian_prev;

    double          ra_cat, dec_cat, pra, pdec, rvel;
    double 	    cmdpmra=0.0,cmdpmdec=0.0;

    double          azoff = 0.0;

    double          eloff = 0.0;

    float           pressure, humidity, temperature;
    float           windspeed, winddirection;

    float           epoch=2000.;

    double          ra0, dec0,radialVelocity0,radialVelocity;

    double          radot, decdot,radialVelocitydot;
	double		radot_prev=0.0;
	double		decdot_prev=0.0;	
	int 	radec_offset_flag=0;

    double          tjd_prev, et_prev, ra_prev, dec_prev,radialVelocity_prev;


    double          tjd, tjd0, tjdn;

    double          az, el, azrate=0., elrate=0., az1, el1 ;

    double          pi, secunit, milliarcsec,microdeg;

    double          hr;

    int             i;

    int             azint, elint, tjdint, azrateint, elrateint;

    int             icount = 0, app_pos_flag = 0;

    short           ret;

    double          hour_angle, hangle1, hangle2;

    short int       hangleint;

    int             nplanet;

    /* The following variables are defined for time calculations */
 int              hours, minutes;	
	double seconds,et,delta=0.,utcsec;

    /* Variables for LST calculation */
    double          d1, dtupi, gst;

    /* variables for actual az */
    double          az_actual, el_actual;
    double          az_actual_disp, el_actual_disp;

    /* end of time variables definitions */

    /* variables used for display part */
    double          lst_disp, ra_disp, dec_disp, ra_cat_disp, dec_cat_disp;

    double          az_disp, el_disp, utc_disp;
    double          az_disp_rm, el_disp_rm;

    double             tjd_disp;

    int             initflag = 0;

    /* source counter */
    int             source = 0;

    int             source_skip_flag = 1;

/*
        int     padid=0;
*/
	char line[BUFSIZ];

    /* the following variables are from name.c */
    char            sname[34];
    char            sname2[34];

    int             sol_sys_flag, id, star_flag;

    struct source   observe_source;

    /* scan flags */
    int             azscan_flag = 0, elscan_flag = 0;
    int             position_switching_flag = 0;
    int             on_source = 0, off_source = 0;
    int             on_source_timer = 0, off_source_timer = 0;

    double          scan_unit = 1.;



    double          Az_cmd, El_cmd, Az_cmd_rate, El_cmd_rate;

    int             az_cmd, el_cmd;

    double          az_actual_msec, el_actual_msec;

    unsigned long          az_actual_msec_int, el_actual_msec_int;

    double          posn_error, az_error, el_error;

    /* for weather parameters */

    short           azoff_int, eloff_int;

    /* integration time for ncam */
    int             integration = DEFAULT_INTEGRATION_TIME;
    short           int integration_short;

    time_t          time1,timeStamp;
    struct tm      *tval,*tm;
    struct timeval tv;
/*    struct timezone tz;*/


    float           pmdaz, pmdel, refraction;
	double		drefraction;

    short            source_on_off_flag = INVALID;
    char            previous_source_on_off_flag = INVALID;
    char            spectrometer, send_spectrometer_interrupt;

    /* tracking error smoothing */
    double          tracking_error, tracking_error_accum = 0.0, tracking_error_buffer[SMOOTH_LENGTH],
                   *dp, smoothed_tracking_error = 0.0;
    int             off_position_counter = 0, off_position_sign;

    short           target_flag = 0;
    double          commanded_az=0., commanded_el=15.;
    double          commanded_az_rate=0.0,commanded_el_rate=0.0;


	short scan_unit_int;

	short errorflag=OK;
	short waitflag=0;
	unsigned short slength;

	double az_actual_corrected;

double et_prev_big_time_step=0.,et_time_interval;

	
/* for sun avoidance: */
	
	double sunaz,sunel;
	int suneloff=0;
	short sun_avoid_flag=0;

/* for IRIG device driver*/
	/*
	int device_fd, irig_status;
	short irig_status_s;
	POS_TIME pt_struct;
	SMA_PT sma_struct;
	DAY_TYPE julianDay;
	*/
	
/*
	int device_fd,irig_status;
	struct vme_sg_simple_time ts;
	struct sc32_time sctime;
	int sc32fd;
*/

/* for earthtilt and sidereal_time */
	double equinoxes,tjd_upper,tjd_lower,dpsi,deps,tobl,mobl;
	
/*
 	float sunazf,sunelf;
*/
	float az_tracking_error,el_tracking_error;
	short dummyshortint;
	float dummyFloat;
	double dummyDouble;
	char dummyByte;
	double hourangle;
	body Planet;
	body earth = {0, 399, "earth"};

	cat_entry star = {"cat","star",0,0.,0.,0.,0.,0.,0.};
	double distance=0.;
	/* planet radii from explanatory supplement physical ephemeredis*/
	double planetradius[11]={0.0,2439.7,6501.9,3200.0,3397.0,71492.0,
			60268.0, 25559.0,24764.,1151.0,1738.0};

	double planetradius301=1738.; /* Moon radius in km*/
	double planetradius501=1820.; /* io radius in km*/
	double planetradius502=1565.; /* europa radius in km*/
	double planetradius503=2634.; /* ganymede radius in km*/
	double planetradius504=2403.; /* callisto radius in km*/
	double planetradius601=200.; /* mimas radius in km*/
	double planetradius602=250.; /* enceladus radius in km*/
	double planetradius603=530.; /* tethys radius in km*/
	double planetradius604=560.; /* dione radius in km*/
	double planetradius605=764.; /* rhea radius in km*/
	double planetradius606=2575.; /* titan radius in km*/
	double planetradius607=145.; /* hyperion radius in km*/
	double planetradius608=718.; /* iapetus radius in km*/

	double planetradius801=1352.6; /* triton radius in km*/
	double planetradius802=170.; /* nereid radius in km */
	double planetradius375=474.; /* ceres radius in km */
	double planetradius376=266.; /* pallas radius in km */
	double planetradius377=203.6; /* hygiea radius in km */
	double planetradius378=265.; /*vesta radius in km */
	/* the above two comet radii are arbitrary- just to avoid junk
	 values for angular diameter*/

	double planetdistance=0.;
	double planetdiameter=0.;
	int beep_flag=0;

	struct utsname unamebuf;

 	/* for getting the system time to find the year */
	/* defined in vme_sg.h */
	/*
	YEAR_TYPE year;	
	*/

#if 0
	/* for ccd image header */
	char snamefits[100];
#endif
	
	float lst_disp_float, utc_disp_float;

	int scbComm=0;
        int az_enc,el_enc;

        int milliseconds;
	int servomilliseconds;
	int checkmsec;
	double dmilliseconds;

        int servoOnFlag=0;

        int az_offset_flag=0;
        int el_offset_flag=0;

        double prev_azrate = 0.;
        double prev_elrate = 0.;
                                      
/*
	char antdir[10];
*/
	
	int azelCommandFlag=0;

	double pos1950[3],pos2000[3];

        time_t timestamp;

	double museconds;
	
	int polar_mjd;
	double polar_dx,polar_dy;
	float polar_dx_f,polar_dy_f;

	
	double raOffset=0.0;
	double decOffset=0.0;
	double cosdec=1.0;

	char modeldate[10];
	char rmodeldate[10];


	char newCmdSourceName[34];
	int newSourceFlag=0;


        site_info location = {latitude_degrees,longitude_degrees,height_m,0.0,0.0};       

	int corruptedMountModelFile=0;
	int end_of_file=0;

/*
	double sundistance=0.;
*/

        /* for ACU ethernet communications */
       struct sockaddr_in serv_addr;

     /* communication of az/el trajectory to ACU for program track mode */
     int numPos,dayNum,timeOfDay[NUMPOS],azProgTrack[NUMPOS],elProgTrack[NUMPOS]; 
     double azT,elT,lstT,tjdT;
     int iT,respACU,firstTrack=0;

     int acuCurrentMode=0;

/*
     int loop_time = 250000;
*/
     int loop_time = 500000;

    /* END OF VARIABLE DECLARATIONS */

    /********Initializations**************************/


	strcpy(sname,"test");
	strcpy(sname2,"test");

/* for stderr buffering- see smainit docs */

/*
DAEMONSET
*/

	 /* First of all, find out if some other instance of
         Track is running */
/*
        if(processPresent("excl_Track"))
        {
        fprintf(stderr,"Track is already running - goodbye.\n");
        exit(1);
        }
*/

/*
	This was hanging up the cpu on giving the resume command.
	Perhaps, all  threads should not be at 50.
*/
/*
	setpriority(PRIO_PROCESS,0,50);
*/

    pi = 4.0 * atan(1.0);
    dtupi = pi * 2.0;
    radian = pi / 180.;
    secunit = 1.15740741e-5;	/* day in one second */
    milliarcsec = 180. / pi * 3600000.; /* radians to milliarcsec*/
    microdeg = 180./pi * 1.0e6;/* radians to microdegrees (for ACU)*/


	 /* signal handler for control C */
        action.sa_flags=0;
        sigemptyset(&action.sa_mask);
        action.sa_handler = handlerForSIGINT;
        sigactionInt = sigaction(SIGINT,&action, &old_action);
        sigactionInt = sigaction(SIGTERM,&action, &old_action);


/* uncomment this when the IRIG-B signal becomes available */
#if 0
	/* initializing the IRIG board */
	  /* get the year from the system time and set it on the
                IRIG device driver */

	sc32fd = open("/dev/syncclock32",O_RDWR,0);
	if(sc32fd<=0) {
	device_fd = open("/dev/vme_sg_simple", O_RDWR,0);     
	   if(device_fd==-1) { 
	   	perror("open()");
		fprintf(stderr,"Could not open vme_sg_simple device");
		exit(SYSERR_RTN);
	   }
        }
#endif



	/*
	irig_status = ioctl(device_fd, SET_YEAR, &year);
	*/

        /* initializing DSM */
        dsm_status=dsm_open();
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_open()");
		fprintf(stderr,"Could not open distributed shared  memory.");
		fprintf(stderr,"Check if dsm is running.");
                exit(-1);
        }

       /* initialize connection to redis */
       redisC = redisConnectWithTimeout(REDIS_SERVER,REDIS_PORT,redisTimeout);
       if (redisC == NULL || redisC->err) {
        if (redisC) {
            printf("Connection error: %s\n", redisC->errstr);
            redisFree(redisC);
          } else {
            printf("Connection error: can't allocate redis context\n");
        }
       }

#if 0
	/* start listening to ref. mem. interrupts for higher-level commands*/
	dsm_status=dsm_monitor(DSM_HOST,"DSM_COMMAND_FLAG_S");
                if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_monitor()");
                exit(1);
                }
#endif 

#if 0
        /* initializing ACU ethernet communications */
        if((sockfdControl = socket(AF_INET, SOCK_STREAM, 0))< 0) {
            printf("\n Error : Could not create socket \n");
            return 1;
          }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ACU_CONTROL_PORT);
        serv_addr.sin_addr.s_addr = inet_addr(ACU_IP_ADDRESS);

        if(connect(sockfdControl, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)           {
            printf("\n Error : Connect Failed \n");
            return 1;
          }

        if((sockfdMonitor = socket(AF_INET, SOCK_STREAM, 0))< 0) {
            printf("\n Error : Could not create socket \n");
            return 1;
          }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ACU_MONITOR_PORT);
        serv_addr.sin_addr.s_addr = inet_addr(ACU_IP_ADDRESS);

        if(connect(sockfdMonitor, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)           {
            printf("\n Error : Connect Failed \n");
            return 1;
          }

#endif 

         // ACU connection retry logic


        // --- Connect to CONTROL port ---
        int attempt = 0;
        while ((sockfdControl = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "[glttrack] Failed to create control socket. Retrying...\n");
            sleep(CONNECT_WAIT_SEC);
            if (++attempt == MAX_CONNECT_RETRIES) {
                fprintf(stderr, "[glttrack] Too many control socket failures. Exiting.\n");
                exit(1);
            }
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ACU_CONTROL_PORT);
        serv_addr.sin_addr.s_addr = inet_addr(ACU_IP_ADDRESS);

        attempt = 0;
        while (connect(sockfdControl, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            fprintf(stderr, "[glttrack] Control port connect failed. Retrying...\n");
            close(sockfdControl);
            sockfdControl = socket(AF_INET, SOCK_STREAM, 0);
            sleep(CONNECT_WAIT_SEC);
            if (++attempt == MAX_CONNECT_RETRIES) {
                fprintf(stderr, "[glttrack] Failed to connect to ACU control after %d attempts. Exiting.\n", MAX_CONNECT_RETRIES);
                exit(1);
            }
        }
        fprintf(stderr, "[glttrack] Connected to ACU CONTROL port.\n");

        // --- Connect to MONITOR port ---
        attempt = 0;
        while ((sockfdMonitor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "[glttrack] Failed to create monitor socket. Retrying...\n");
            sleep(CONNECT_WAIT_SEC);
            if (++attempt == MAX_CONNECT_RETRIES) {
                fprintf(stderr, "[glttrack] Too many monitor socket failures. Exiting.\n");
                exit(1);
            }
        }
        
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ACU_MONITOR_PORT);
        serv_addr.sin_addr.s_addr = inet_addr(ACU_IP_ADDRESS);
        
        attempt = 0;
        while (connect(sockfdMonitor, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            fprintf(stderr, "[glttrack] Monitor port connect failed. Retrying...\n");
            close(sockfdMonitor);
            sockfdMonitor = socket(AF_INET, SOCK_STREAM, 0);
            sleep(CONNECT_WAIT_SEC);
            if (++attempt == MAX_CONNECT_RETRIES) {
                fprintf(stderr, "[glttrack] Failed to connect to ACU monitor after %d attempts. Exiting.\n", MAX_CONNECT_RETRIES);
                exit(1);
            }
        }
        fprintf(stderr, "[glttrack] Connected to ACU MONITOR port.\n");
        
 
	pthread_attr_init(&attr);
	if (pthread_create(&CommandHandlerTID, &attr, CommandHandler,
			 (void *) 0) == -1) { 
	perror("main: pthread_create CommandHandler");
	exit(-1);
	}
	param.sched_priority=40;
	pthread_attr_setschedparam(&attr,&param);
	pthread_setschedparam(CommandHandlerTID,policy,&param);

        sleep(2);
	pthread_attr_init(&attr4);
	if (pthread_create(&ACUiostatusTID, &attr4, ACUiostatus,
			 (void *) 0) == -1) { 
	perror("main: pthread_create ACUiostatus");
	exit(-1);
	}
	param4.sched_priority=15;
	pthread_attr_setschedparam(&attr4,&param4);
	pthread_setschedparam(ACUiostatusTID,policy,&param4);
        fprintf(stderr,"[glttrack] Starting ACUiostatus thread.\n");

        sleep(2);
	pthread_attr_init(&attr2);
	if (pthread_create(&ACUstatusTID, &attr2, ACUstatus,
			 (void *) 0) == -1) { 
	perror("main: pthread_create ACUstatus");
	exit(-1);
	}
	param2.sched_priority=20;
	pthread_attr_setschedparam(&attr2,&param2);
	pthread_setschedparam(ACUstatusTID,policy,&param2);
        fprintf(stderr,"[glttrack] Starting ACUstatus thread.\n");

        sleep(2);
	pthread_attr_init(&attr3);
	if (pthread_create(&ACUmetrologyTID, &attr3, ACUmetrology,
			 (void *) 0) == -1) { 
	perror("main: pthread_create ACUmetrology");
	exit(-1);
	}
	param3.sched_priority=15;
	pthread_attr_setschedparam(&attr3,&param3);
	pthread_setschedparam(ACUmetrologyTID,policy,&param3);
        fprintf(stderr,"[glttrack] Starting ACUmetrology thread.\n");

        sleep(2);
        pthread_attr_init(&watchdogAttr);
        if (pthread_create(&watchdogTID, &watchdogAttr, ACUConnectionWatchdog, 
            NULL) != 0) {
            fprintf(stderr, "Failed to start ACUConnectionWatchdog thread.\n");
        }
        watchdogParam.sched_priority = 10;
        pthread_attr_setschedparam(&watchdogAttr, &watchdogParam);
        pthread_setschedparam(watchdogTID, SCHED_FIFO, &watchdogParam);
        fprintf(stderr,"[glttrack] Starting ACU connection watchdog thread.\n");
        sleep(2);


    /* tracking smoothing error */

    for (i = 0; i < SMOOTH_LENGTH; i++)
	tracking_error_buffer[i] = 0.0;
    dp = tracking_error_buffer;

    /*
     * setting these proper-motion and radial vel terms to zero for now
     */
    pra = 0.0;
    pdec = 0.0;
    rvel = 0.0;


	radio_flag=1;

#if SERVO
	 tsshm = OpenShm(TSSHMNAME, TSSHMSZ);    


	/* Now check if servo is running */
	 checkmsec=tsshm->msec;
	usleep(100000);
	if((tsshm->msec)==checkmsec) {
	fprintf(stderr,"Warning: Servo is not running.\n");
	}
#endif


    /********************end of initializations*******************/

	/* on first pass, make cmd posn = actual posn. */

	if(icount==0)
	{

#if SERVO
	az_enc = tsshm->encAz;
        el_enc = tsshm->encEl;     
#else
	az_enc = 90.0;
	el_enc = 20.0;
#endif
/*
	az_actual=(double)az_enc/MSEC_PER_DEG;
        el_actual=(double)el_enc/MSEC_PER_DEG;       
*/
        az_actual=az_enc_from_acu;
        el_actual=el_enc_from_acu;

	/* call local to get mount model corrections for the first
		commanded position */
	ra = 0.;
        dec = 0.;
        ra_disp = 0.;
        dec_disp = 0.;
        ra_cat_disp = 0.;
        dec_cat_disp = 0.;
        ra0 = 0.;
        dec0 = 0.;
        radot = 0.;
        decdot = 0.;
        hangle1 = 0.;
        hangle2 = 0.;
	lst_radian_prev=0;
	tjd_prev=0.;
        dsm_status=dsm_read(DSM_HOST,"DSM_AZ_POSN_DEG_D",&az_actual,&timeStamp);
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_read() DSM_AZ_POSN_DEG_D");
                exit(1);
        }
        dsm_status=dsm_read(DSM_HOST,"DSM_EL_POSN_DEG_D",&el_actual,&timeStamp);
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_read() DSM_EL_POSN_DEG_D");
                exit(1);
        }
	commanded_az=az_actual;
	commanded_el=el_actual;

	target_flag=1;
        local(&lst_radian_prev, &ra, &dec, &az, &el, &tjd_prev, &azoff,
		&eloff, &pressure, &temperature, &humidity, &radio_flag, 
		&refraction, &pmdaz, &pmdel,
		&target_flag,&commanded_az,&commanded_el);
        commanded_el = el_actual-pmdel/3600.;
	commanded_az = az_actual-pmdaz/3600./cos(commanded_el*radian);
	strcpy(sname,"target");
	} /* if icount==0 */

new_source:                    
        firstTrack=1; /*loop counter for az/el values to ACU */
	  /* read polar wobble parameters 
	     from the file /global/polar/polar.dat */

        fp_polar = fopen("/global/polar/polar.dat","r");
        if(fp_polar==NULL) {
        fprintf(stderr,"Failed to open polar.dat file\n");
        exit(-1);
        }
	fscanf(fp_polar,"%d %lf %lf %lf",
		&polar_mjd,&polar_dx,&polar_dy,&polar_dut);
	/* write them to DSM */
        polar_dut_f = (float)polar_dut;
        polar_dx_f = (float)polar_dx;
        polar_dy_f = (float)polar_dy;
        dsm_status=dsm_write(DSM_HOST,"DSM_POLAR_MJD_L",&polar_mjd);
        dsm_status=dsm_write(DSM_HOST,"DSM_POLAR_DX_ARCSECONDS_F",&polar_dx_f);
        dsm_status=dsm_write(DSM_HOST,"DSM_POLAR_DY_ARCSECONDS_F",&polar_dy_f);
        dsm_status=dsm_write(DSM_HOST,"DSM_POLAR_DUT_SECONDS_F",&polar_dut_f);
	fclose(fp_polar);

/*
        redisWriteInt("gltTrackFile","polarMJD",polar_mjd);
        redisWriteFloat("gltTrackFile","polarDX",polar_dx_f);
        redisWriteFloat("gltTrackFile","polarDY",polar_dy_f);
        redisWriteFloat("gltTrackFile","polarDUT",polar_dut_f);
*/


    print_upper(sname);

    if (target_flag==1) strcpy(sname,"target");

    if (target_flag == 0)
    {

	 strcpy(messg, "                              ");
                SendMessageToDSM(messg);

	strcpy(sname2,sname);
	pad(sname, 20);

	if(newSourceFlag==0) is_planet(sname, &sol_sys_flag, &id);

	if (sol_sys_flag == 1)
	{
	  
	    nplanet = id;
	
		/* in the new ephemeris codes, moon is 10, not 301 */
		/*
		if(nplanet==301) nplanet=10;
		commented out on 11 jan 2001, now we are back to
		ansi-C codes from jpl and not using hoffman's package
		to read the jpl ephemeris files*/
	    ra_cat_disp = 0.;
	    dec_cat_disp = 0.;
	   Planet.type=0;
	   Planet.number=nplanet;
	  strcpy(Planet.name,sname);
	  strcpy(sptype,"----------");
	  sptype[9]=0x0;
	  magnitude=0.0;
	}
	if (sol_sys_flag == 0)
	{
            if(newSourceFlag==0) starcat(sname, &star_flag, &observe_source);
            if(newSourceFlag==1) star_flag=1;

	    if (star_flag == 0)
	    {
		nplanet = 0;

	/* unknown source. go into target mode with current

	position as commanded position */

	       strcpy(sname,"unknown");
		strcat(messg,"Source not found.");
                SendMessageToDSM(messg);
		fprintf(stderr,"%s\n",messg);
          printf("Error: %s\n",operatorErrorMessage);

#if SERVO
        az_enc = tsshm->encAz;
        el_enc = tsshm->encEl;
#else
	az_enc = 90.0;
	el_enc = 20.0;
#endif
/*
        az_actual=(double)az_enc/MSEC_PER_DEG;
        el_actual=(double)el_enc/MSEC_PER_DEG;
*/
        dsm_status=dsm_read(DSM_HOST,"DSM_AZ_POSN_DEG_D",&az_actual,&timeStamp);
        dsm_status=dsm_read(DSM_HOST,"DSM_EL_POSN_DEG_D",&el_actual,&timeStamp);
       /* call local to get mount model corrections for the first
                commanded position */
        ra = 0.;
        dec = 0.;
        ra_disp = 0.;
        dec_disp = 0.;
        ra_cat_disp = 0.;
        dec_cat_disp = 0.;
        ra0 = 0.;
        dec0 = 0.;
        radot = 0.;
        decdot = 0.;
        hangle1 = 0.;
        hangle2 = 0.;
        lst_radian_prev=0;
        tjd_prev=0.;
        commanded_az=az_actual;
        commanded_el=el_actual;
        target_flag=1;
        local(&lst_radian_prev, &ra, &dec, &az, &el, &tjd_prev, &azoff,
                &eloff, &pressure, &temperature, &humidity, &radio_flag,
                &refraction, &pmdaz, &pmdel,
                &target_flag,&commanded_az,&commanded_el);
        commanded_el = el_actual-pmdel/3600.;
        commanded_az = az_actual-pmdaz/3600./cos(commanded_el*radian);


	    }

	    if (star_flag == 1)
	    {
		nplanet = 0;

		if(newSourceFlag==0) {

		dec_cat = fabs(observe_source.decd) + observe_source.decm / 60. + observe_source.decs / 3600.;
	dec_cat = dec_cat + decOffset/3600.;
		cosdec=cos(dec_cat*radian);
	
		ra_cat = observe_source.rah + observe_source.ram / 60. + observe_source.ras / 3600.;
		ra_cat = ra_cat + (raOffset/3600./15.0/cosdec);

		if (observe_source.decsign == '-') dec_cat = -dec_cat;

/* if the coordinates are B1950, precess them to J2000 first*/
		epoch = observe_source.epoch;
		} /* if newSourceFlag==0; source from catalog */

		if(newSourceFlag==1) {
		dsm_status=dsm_read(DSM_HOST,"DSM_CMD_SOURCE_C34",newCmdSourceName,&timeStamp);
		strcpy(sname,newCmdSourceName);
		dsm_status=dsm_read(DSM_HOST,"DSM_CMD_RA_HOURS_D",&ra_cat,&timeStamp);
		dsm_status=dsm_read(DSM_HOST,"DSM_CMD_DEC_DEG_D",&dec_cat,&timeStamp);
		dsm_status=dsm_read(DSM_HOST,"DSM_CMD_PMRA_MASPYEAR_D",&cmdpmra,&timeStamp);
		dsm_status=dsm_read(DSM_HOST,"DSM_CMD_PMDEC_MASPYEAR_D",&cmdpmdec,&timeStamp);
		dsm_status=dsm_read(DSM_HOST,"DSM_CMD_EPOCH_YEAR_D",&dummyDouble,&timeStamp);
		epoch=(float)dummyDouble;
		dsm_status=dsm_read(DSM_HOST,"DSM_CMD_SVEL_KMPS_D",&sourceVelocity,&timeStamp);

		dec_cat =  dec_cat + decOffset/3600.;
		cosdec=cos(dec_cat*radian);
	
		ra_cat = ra_cat + (raOffset/3600./15.0/cosdec);

		}/* if source is not from catalog but given via observe command. */

		if (epoch == 1950.)
		{
			radec2vector(ra_cat,dec_cat,1.0,pos1950);
                        precession(T1950,pos1950,T2000,pos2000);
                        vector2radec(pos2000,&ra_cat,&dec_cat);

		}
		star.ra=ra_cat;
		star.dec=dec_cat;

		/* converting input pm-ra in mas/yr to sec/century */
/* possible bug- found 5 jul 2006
		star.promora= cmdpmra/15./cos(dec_cat*radian)/10.;
		star.promodec=cmdpmdec/10.;
		if(newSourceFlag==1) {
		star.promora=
		 (double)observe_source.pmr/15./cos(dec_cat*radian)/10.;
		star.promodec=(double)observe_source.pmd/10.;
		}
*/

		if(newSourceFlag==1) {
		star.promora= cmdpmra/15./cos(dec_cat*radian)/10.;
		star.promodec=cmdpmdec/10.;
		} else {
		star.promora=
		 (double)observe_source.pmr/15./cos(dec_cat*radian)/10.;
		star.promodec=(double)observe_source.pmd/10.;
		}

		ra_cat_disp = ra_cat;
		dec_cat_disp = dec_cat;
	    }			/* star_flag if */
	}			/* sol_sys_flag if */
    }				/* if target flag is 0 */
/* write sol_sys_flag to DSM */
        dummyshortint=(short)sol_sys_flag;
        dsm_status=dsm_write(DSM_HOST,"DSM_SOLSYS_FLAG_S",&dummyshortint);
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_write() solsysflag");
                }
/*
        redisWriteShort("gltTrackComp","solsysFlag",dummyshortint);
*/
 






/************************************************************************/
    /* starting infinite loop */
/************************************************************************/


    /* getting previously written offsets from ref. mem. */

	dsm_status=dsm_read(DSM_HOST,"DSM_AZOFF_ARCSEC_D",&azoff,&timeStamp);	
	dsm_status=dsm_read(DSM_HOST,"DSM_ELOFF_ARCSEC_D",&eloff,&timeStamp);	
	dsm_status=dsm_read(DSM_HOST,"DSM_RAOFF_ARCSEC_D",&raOffset,&timeStamp);
	dsm_status=dsm_read(DSM_HOST,"DSM_DECOFF_ARCSEC_D",&decOffset,&timeStamp);

/*
        redisWriteDouble("gltTrackuser","raoff",raOffset);
        redisWriteDouble("gltTrackuser","decoff",decOffset);
*/


    /*
     * if offsets are huge, due to a bad initialization in ref. mem. then do
     * not use them if(fabs(azoff)>1800.) azoff=0.; if(fabs(eloff)>1800.)
     * azoff=0.;
     */

    /* begin while loop every loop_time musecond */
    while (1)
    {

beginning:


    /* read mount model parameters */
	if(read_mount_model_flag==1) {
	fp_mount_model=fopen("pointingModel","r"); 
	if(fp_mount_model==NULL) {
	fprintf(stderr,"Failed to open the mount model file.\n");
	exit(-1);
	}
	end_of_file=0;
	numParameters=0;
	corruptedMountModelFile=0;

        while(fgets(line,sizeof(line),fp_mount_model) != NULL) {
        line[strlen(line)-1]='\0';
                if(line[0]!='#') {
        sscanf(line,"%s %s %s", pointingParameter, opticalValue, radioValue);
                     if(!strcmp(pointingParameter,"AzDC")) {
                     azdc=atof(opticalValue);
                     razdc=atof(radioValue);
                        numParameters++;
                     }

                     if(!strcmp(pointingParameter,"AzColl")) {
                     azcol=atof(opticalValue);
                     razcol=atof(radioValue);
                        numParameters++;
                     }

                     if(!strcmp(pointingParameter,"ElTilt")) {
                     eltilt=atof(opticalValue);
                     reltilt=atof(radioValue);
                        numParameters++;
                     }

                     if(!strcmp(pointingParameter,"AAzTltSin")) {
                     aztilt_sin=atof(opticalValue);
                     raztilt_sin=atof(radioValue);
                        numParameters++;
                     }

                     if(!strcmp(pointingParameter,"AAzTltCos")) {
                     aztilt_cos=atof(opticalValue);
                     raztilt_cos=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AAzTltSin2")) {
                     aztilt_sin2=atof(opticalValue);
                     raztilt_sin2=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AAzTltCos2")) {
                     aztilt_cos2=atof(opticalValue);
                     raztilt_cos2=atof(radioValue);
                        numParameters++;
                     }

                     if(!strcmp(pointingParameter,"AzEncSin")) {
                     azenc_sin=atof(opticalValue);
                     razenc_sin=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AzEncCos")) {
                     azenc_cos=atof(opticalValue);
                     razenc_cos=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AzEncSin2")) {
                     azenc_sin2=atof(opticalValue);
                     razenc_sin2=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AzEncCos2")) {
                     azenc_cos2=atof(opticalValue);
                     razenc_cos2=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AzEncSin3")) {
                     azenc_sin3=atof(opticalValue);
                     razenc_sin3=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AzEncCos3")) {
                     azenc_cos3=atof(opticalValue);
                     razenc_cos3=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"AzRms")) {
                     azmodelrms=atof(opticalValue);
                     razmodelrms=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"ElDC")) {
                     eldc=atof(opticalValue);
                     reldc=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"ElSag")) {
                     elsag=atof(opticalValue);
                     relsag=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"ElSagSin")) {
                     elsagsin=atof(opticalValue);
                     relsagsin=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"ElSagTan")) {
                     elsagtan=atof(opticalValue);
                     relsagtan=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"EAzTltSin")) {
                     eaztilt_sin=atof(opticalValue);
                     reaztilt_sin=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"EAzTltCos")) {
                     eaztilt_cos=atof(opticalValue);
                     reaztilt_cos=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"EAzTltSin2")) {
                     eaztilt_sin2=atof(opticalValue);
                     reaztilt_sin2=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"EAzTltCos2")) {
                     eaztilt_cos2=atof(opticalValue);
                     reaztilt_cos2=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"ElRms")) {
                     elmodelrms=atof(opticalValue);
                     relmodelrms=atof(radioValue);
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"TiltFlag")) {
                     defaultTiltFlag=(int)atof(opticalValue);
                     rdefaultTiltFlag=(int)atof(radioValue);
		     setTiltFlag=defaultTiltFlag;
                        numParameters++;
                     }
                     if(!strcmp(pointingParameter,"Date")) {
                     strcpy(modeldate,opticalValue);
                     strcpy(rmodeldate,radioValue);
                        numParameters++;
                     }

                }

        }

	if(numParameters!=25) corruptedMountModelFile=1;

	fclose(fp_mount_model);
	if(corruptedMountModelFile==1) {
	strcpy(messg, "Mount model file is corrupted.");
        SendMessageToDSM(messg);
        fprintf(stderr,"%s\n",messg);
        strcpy(operatorErrorMessage, "Mount model file is corrupted.");
/*
          sendOpMessage(OPMSG_WARNING, 10, 30, operatorErrorMessage);
*/
          printf("Error: %s\n",operatorErrorMessage);
	exit(-1);
	}

	read_mount_model_flag=0;
	}

/*
printf("%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %s",
razdc,razcol,reltilt,raztilt_sin,raztilt_cos,raztilt_sin2,raztilt_cos2,
razenc_sin,razenc_cos,razenc_sin2,razenc_cos2,razenc_sin3,razenc_cos3,
razmodelrms,reldc,relsag,reaztilt_sin,reaztilt_cos,reaztilt_sin2,reaztilt_cos2,relmodelrms,rmodeldate);
*/


#if 0
	/* get sun's position */

	dsm_status=dsm_read(DSM_HOST,"DSM_SUN_AZ_DEG_F",&sunazf,&timeStamp);
	sunaz=(double)sunazf;
	sunaz=sunaz*radian;

	dsm_status=dsm_read(DSM_HOST,"DSM_SUN_EL_DEG_F",&sunelf,&timeStamp);
	sunel=(double)sunelf;
	sunel=sunel*radian;
#endif


	if (source_skip_flag == 1) {
	    icount = 0;
	    source_skip_flag = 0;
	    source++;
	}

	icount++;

	if ((icount % BIG_TIME_STEP) == 0) {
	    app_pos_flag = 1;
	} else {
	    app_pos_flag = 0;
	}


	/*-------------------------------time-------------------------*/

		

/*
	hr=(double)ts.hour+((double)ts.min)/60.+ 
			(((double)ts.sec +(double)ts.usec)/1.0e6)/3600.;
*/

/* uncomment this when IRIG-B is available */
#if 0

	   if(sc32fd>0) {

		 if((irig_status = read(sc32fd, &sctime, sizeof(sctime))) < 0) {
            	fprintf(stderr, "Error %d reading Syncclock32\n", irig_status);
            	}


	    hours = sctime.hour;
	    minutes = sctime.min;
	    seconds = (double) sctime.sec;
	    museconds = (double) sctime.usec;

	   } else {


	      irig_status = read(device_fd,&ts,1);
	        if(irig_status==-1) {
		perror("read()");
		exit(-1);
	       }
 
	    hours = ts.hour;
	    minutes = ts.min;
	    seconds = (double) ts.sec;
	    museconds = (double) ts.usec;
	    }
#endif

/* for now, reading time from system clock */
		gettimeofday(&tv, NULL);
		time(&time1);
		tm=gmtime(&time1);
/*
		tm=localtime(&tv.tv_sec);
*/
		hours = tm->tm_hour;
		minutes = tm->tm_min;
		seconds = tm->tm_sec;
		museconds = tv.tv_usec;
	   tjd_upper = (double)((int)(365.25*((tm->tm_year+1900)+4711))) + 
			(double)tm->tm_yday + 351.5;

           tjd_upper += 1.0; /* tm_yday is from 0 to 365 */

/*
printf("tjd_upper: %f, year: %d , day: %d\n",tjd_upper,tm->tm_year,tm->tm_yday);
printf(" %d:%02d:%02d %d \n", tm->tm_hour, tm->tm_min,tm->tm_sec, tv.tv_usec);
*/

	    seconds += (museconds/1.0e6);

	hr = (double)hours + ((double)minutes)/60. + seconds/3600.;
	    utc_disp = hr;
	dmilliseconds=hr * 3600000.;
	milliseconds = (int) dmilliseconds;
/* calculating julian day */
/*
	   tjd_upper=(double)julianDay-0.5;
*/


	   tjd_lower=hr/24.;

 	   tjd_lower += polar_dut/86400.;

	   tjd=tjd_upper+tjd_lower;


/*
printf("tjd=%lf \n",tjd);
*/

   utcsec = (tjd-2451545.)* 86400.0;
	
/*
	  fprintf(stderr,"The GPS (UTC) time is: %d:%d:%d:%lf\n",
		julianDay,hours,minutes,seconds);
*/


/*
	    delta=32.184+LEAPSECONDS+polar_dut;
*/
	    delta=32.184+LEAPSECONDS;

	    et = utcsec+delta ;

	 /* obtain equation of equinoxes which is needed for LST */
	cel_pole(polar_dx,polar_dy);
	earthtilt(tjd_upper,&mobl, &tobl, &equinoxes,&dpsi, &deps);
	sidereal_time(tjd_upper, tjd_lower,equinoxes, &gst);
	d1 = gst *dtupi / 24.+longrad;
	lst_radian = fmod(d1, dtupi);
	if (lst_radian < 0.) {
	    lst_radian += dtupi;
	}
	/* converting lst to hours */
	lst = lst_radian * 24. / dtupi;
/*
	lst += polar_dut/3600.;
*/
	lst_disp = lst;
	tjd_disp = tjd;

/*
printf("lst=%lf\n",lst_disp);
printf("tjd_disp=%d\n",tjd_disp);
*/


	/*--------------------end of time part------------------------*/

	/*--------------------beginning of apparent calculations------*/

	if (target_flag == 0) {

	    if ((app_pos_flag == 1) || (icount == 1))
	    {

		if (nplanet != 0)
		{
		   topo_planet(tjd, &Planet,&earth, delta, 
			&location, &ra0, &dec0, &distance,&radialVelocity0);

	/*increment ra0 and dec0 with offsets */
	dec0 = dec0 + decOffset/3600.;
	cosdec=cos(dec0*radian);
	ra0=ra0+(raOffset/3600./15.0/cosdec);
	
		radialVelocity0 *= AUPERDAY_KMPERSEC;
		}
		if (nplanet == 0)
		{
		    topo_star(tjd,&earth,delta, &star,&location, &ra0, &dec0);

		}
		if (icount == 1)
		{
		    tjd_prev = tjd - secunit * 30.;
		    et_prev = et - 30.;
		    lst_prev = lst - 8.3333333333e-3; 
		    lst_radian_prev = lst_prev * dtupi / 24.;
		    if (lst_radian_prev < 0.)
		    {
			lst_radian_prev += dtupi;
		    }
		    if (nplanet != 0)
		    {
		   topo_planet(tjd_prev, &Planet,&earth, delta, 
		&location, &ra_prev, &dec_prev, &distance,&radialVelocity_prev);
		radialVelocity_prev *= AUPERDAY_KMPERSEC;


	if(Planet.number <=11)  {
	planetdiameter=2.0*planetradius[Planet.number]/distance/149597900.0;
	} else {
	switch (Planet.number) {
	case 301:
	planetdiameter=2.0*planetradius301/distance/149597900.0;
	break;
	case 501:
	planetdiameter=2.0*planetradius501/distance/149597900.0;
	break;
	case 502:
	planetdiameter=2.0*planetradius502/distance/149597900.0;
	break;
	case 503:
	planetdiameter=2.0*planetradius503/distance/149597900.0;
	break;
	case 504:
	planetdiameter=2.0*planetradius504/distance/149597900.0;
	break;
	case 601:
	planetdiameter=2.0*planetradius601/distance/149597900.0;
	break;
	case 602:
	planetdiameter=2.0*planetradius602/distance/149597900.0;
	break;
	case 603:
	planetdiameter=2.0*planetradius603/distance/149597900.0;
	break;
	case 604:
	planetdiameter=2.0*planetradius604/distance/149597900.0;
	break;
	case 605:
	planetdiameter=2.0*planetradius605/distance/149597900.0;
	break;
	case 606:
	planetdiameter=2.0*planetradius606/distance/149597900.0;
	break;
	case 607:
	planetdiameter=2.0*planetradius607/distance/149597900.0;
	break;
	case 608:
	planetdiameter=2.0*planetradius608/distance/149597900.0;
	break;
	case 801:
	planetdiameter=2.0*planetradius801/distance/149597900.0;
	break;
	case 802:
	planetdiameter=2.0*planetradius802/distance/149597900.0;
	break;
	case 375:
	planetdiameter=2.0*planetradius375/distance/149597900.0;
	break;
	case 376:
	planetdiameter=2.0*planetradius376/distance/149597900.0;
	break;
	case 377:
	planetdiameter=2.0*planetradius377/distance/149597900.0;
	break;
	case 378:
	planetdiameter=2.0*planetradius378/distance/149597900.0;
	break;
	}

	}
	planetdiameter=(planetdiameter/radian)*3600.;

		    } else
		    {
		    topo_star(tjd_prev,&earth,delta, &star,&location, &ra_prev, &dec_prev);
		    }
		}

		et_time_interval = et - et_prev_big_time_step;

		if(icount==1) et_time_interval=30.;

		if(radec_offset_flag==0) {
		radot = (ra0 - ra_prev) / 30.;
		decdot = (dec0 - dec_prev) /30. ;
		} else {
		radot = radot_prev;
		decdot = decdot_prev;
		radec_offset_flag=0;
		}
		radialVelocitydot=(radialVelocity0-radialVelocity_prev)/30.;

		    ra_prev = ra0;
		    dec_prev = dec0;
		    radot_prev=radot;
		    decdot_prev=decdot;
		    radialVelocity_prev=radialVelocity0;
		    et_prev_big_time_step=et;

		
	    }			/* ((app_pos_flag == 1) || (icount == 1)) */
	}			/* if target_flag=0 */

	/* read the weather parameters from ref. mem. */
	dsm_status=dsm_read(DSM_HOST,"DSM_WEATHER_TEMP_C_F",&temperature,&timeStamp);
	dsm_status=dsm_read(DSM_HOST,"DSM_WEATHER_HUMIDITY_F",&humidity,&timeStamp);
	dsm_status=dsm_read(DSM_HOST,"DSM_WEATHER_PRESS_MBAR_F",&pressure,&timeStamp);
	dsm_status=dsm_read(DSM_HOST,"DSM_WEATHER_WINDSPEED_MPS_F",&windspeed,&timeStamp);
	dsm_status=dsm_read(DSM_HOST,"DSM_WEATHER_WINDDIR_AZDEG_F",&winddirection,&timeStamp);

	/*windspeed *= 0.44704;  convert from mph to m/s */


	/* check for limits on weather parameters */
	if (
	    (pressure < 600.) || (pressure > 1200.)
	    || (temperature < -80.) || (temperature > 70.)
	    || (humidity < 0.) || (humidity > 110.)
	    ) {
		strcpy(messg, "                                   ");
		SendMessageToDSM(messg);
		strcpy(messg, "Check the weather station.");
		SendMessageToDSM(messg);

	}
#if 0
/* Putting fiducial values of weather for now-
*/
	temperature = 35.0; /* C */
	pressure = 1015.5 ; /* mbar */
	humidity = 14.0; /* % */
	windspeed = 0.0;
	winddirection = 0.0;
#endif 

	/*
	 * ra and dec in hours and degrees, apparent coordinates output from
	 * stars or planets functions are converted below into azimuth and
	 * elevation. First, linearly interpolating between the ra and dec
	 * apparent positions, calculated every BIG_TIME_STEPs
	 */
	if (target_flag == 0) {
	    if(icount!=1)
            {		
	    ra = ra0 + radot * (et-et_prev_big_time_step);
	    dec = dec0 + decdot * (et-et_prev_big_time_step);
	    radialVelocity=radialVelocity0+
			radialVelocitydot*(et-et_prev_big_time_step);
            }		

	    if(icount==1)
	    {
	    ra=ra0;
	    dec=dec0;
	    radialVelocity=radialVelocity0;
	    }		

	    ra_disp = ra;
	    dec_disp = dec;

	    /* if it is a solar system object, replace
                ra_cat, dec_cat by the apparent coordinates
                instead of making them zeroes
                */

                if(sol_sys_flag==1) {
                ra_cat_disp=ra_disp;
                dec_cat_disp=dec_disp;
                }



	    ra = ra * 15.0 * pi / 180.;
	    dec = dec * pi / 180.;


	    if (icount == 1)
	    {
		tjdn = tjd - secunit;
		local(&lst_radian_prev, &ra, &dec, &az, &el, &tjd_prev, &azoff, &eloff, &pressure, &temperature, &humidity, &radio_flag, &refraction, &pmdaz, &pmdel,&target_flag,&commanded_az,
	&commanded_el);
		hangle1 = lst_radian_prev - ra;

		az1 = az;
		el1 = el;
		tjd0 = tjd;
	    }
	}			/* if target flag = 0 */

    if (target_flag == 1) {
	ra = 0.;
	dec = 0.;
	ra_disp = 0.;
	dec_disp = 0.;
	ra_cat_disp = 0.;
	dec_cat_disp = 0.;
	ra0 = 0.;
	dec0 = 0.;
	radot = 0.;
	decdot = 0.;
	strcpy(sptype, "----------");
	sptype[9]=0x0;
	magnitude=0.0;
	hangle1 = 0.;
	hangle2 = 0.;
	sol_sys_flag=0;
	local(&lst_radian_prev, &ra, &dec, &az, &el, &tjd_prev, &azoff, &eloff, &pressure, &temperature, &humidity, &radio_flag, &refraction, &pmdaz, &pmdel,&target_flag,&commanded_az,&commanded_el);
    }

	/* adding offsets for scanning/mapping */
	if (azscan_flag == 1)
	    /* azoff = azoff + scan_unit * TIME_STEP ; */
	    azoff = azoff + scan_unit * TIME_STEP * loop_time/1000000.0;
	if (elscan_flag == 1)
	    /* eloff = eloff + scan_unit * TIME_STEP ; */
	    eloff = eloff + scan_unit * TIME_STEP * loop_time/1000000.0;

	/* position switching */

	if (position_switching_flag == 1) {

	    if (source_on_off_flag != INVALID)
	    {
		if (on_source)
		    on_source_timer--;
		if (off_source)
		    off_source_timer--;
	    }
	    if (on_source_timer == 0)
	    {
		off_position_counter++;
		on_source = 0;
		off_source = 1;
		on_source_timer = integration * 10;
		if ((off_position_counter % 2) == 0)
		    off_position_sign = -1;
		else
		    off_position_sign = 1;
		azoff = azoff + (off_position_sign * scan_unit) ;
	        az_offset_flag=1;
		strcpy(messg, "                                       ");
		SendMessageToDSM(messg);
		strcpy(messg, "position switching - OFF source");
		SendMessageToDSM(messg);
		source_on_off_flag = OFFSOURCE;
		send_spectrometer_interrupt = 1;
	    }
	    if (off_source_timer == 0)
	    {
		on_source = 1;
		off_source = 0;
		off_source_timer = integration * 10;
		azoff = azoff - (off_position_sign * scan_unit);
		az_offset_flag=1;
		strcpy(messg, "                                       ");
		SendMessageToDSM(messg);
		strcpy(messg, "position switching - ON source");
		SendMessageToDSM(messg);
		source_on_off_flag = ONSOURCE;
		send_spectrometer_interrupt = 1;
	    }
	}


     if (target_flag == 0) {
	    local(&lst_radian, &ra, &dec, &az, &el, &tjd, &azoff, &eloff, &pressure, &temperature, &humidity, &radio_flag, &refraction, &pmdaz, &pmdel,&target_flag,&commanded_az,&commanded_el);
	    hangle2 = lst_radian - ra;

/* repeat the call to local() for az/el values computed for future time
values - ACU needs a trajectory of at least 4 values in future, for its
cubic spline interpolation. The number of values is defined as NUMPOS.
If this is the first time after source change (firstTrack=1) then compute 
NUMPOS values in future and pass the array to ACU. Else, compute
(NUMPS+1)th value and pass the az/el to ACU to accumulate in its stack */
       if (firstTrack==1) {
        for(iT=0;iT<NUMPOS;iT++){
        lstT=lst_radian+(iT+loop_time/1.0e6)*7.272205e-5; /*secsPerRadians*/
        tjdT=tjd + (iT+loop_time/1.0e6)/86400.;
	    local(&lstT, &ra, &dec, &azT,&elT,&tjdT,&azoff, &eloff, &pressure, &temperature, &humidity, &radio_flag, &refraction, &pmdaz, &pmdel,&target_flag,&commanded_az,&commanded_el);
        /* correct az for cable wrap; see below*/
/*
          if (azT/radian >260.) azT-=(2.*pi);
          if (azT/radian <-260.) azT+=(2.*pi); 
*/
          if (azT/radian > AZ_CW_LIMIT) azT-=(2.*pi);
          if (azT/radian < AZ_CCW_LIMIT) azT+=(2.*pi); 
        timeOfDay[iT]=milliseconds+(iT+loop_time/1.0e6)*1000;
        azProgTrack[iT]=(int)round(azT*microdeg);
        elProgTrack[iT]=(int)round(elT*microdeg);
	}
       } else {
        lstT=lst_radian+(NUMPOS+2)*(loop_time/1.0e6)*7.272205e-5; /*secsPerRadians*/
        tjdT=tjd + (NUMPOS+2)*(loop_time/1.0e6)/86400.;
	    local(&lstT, &ra, &dec, &azT,&elT,&tjdT,&azoff, &eloff, &pressure, &temperature, &humidity, &radio_flag, &refraction, &pmdaz, &pmdel,&target_flag,&commanded_az,&commanded_el);
        /* correct az for cable wrap; see below*/
/*
          if (azT/radian >260.) azT-=(2.*pi);
          if (azT/radian <-260.) azT+=(2.*pi); 
*/
          if (azT/radian > AZ_CW_LIMIT) azT-=(2.*pi);
          if (azT/radian < AZ_CCW_LIMIT) azT+=(2.*pi); 
        timeOfDay[0]=milliseconds+(NUMPOS+2)*(loop_time/1.0e6)*1000;
        azProgTrack[0]=(int)round(azT*microdeg);
        elProgTrack[0]=(int)round(elT*microdeg);
       }
      }

	/* these variables are for the display */
	el_disp = el-pmdel/3600.*radian;
	az_disp = az-pmdaz/3600.*radian/cos(el_disp);


/* Remove the mount model from the commanded positions for display,
since these should be the true positions (also remove similarly,
for the actual positions*/

#if 0
	/* Compare az_disp and el_disp with Sun's az and el
	and go into simulation mode with an error message
	if the commanded position is within SUNLIMIT degrees
	of the Sun's position */

	sundistance=sunDistance(az_disp,el_disp,sunaz,sunel);
	dsm_write(DSM_HOST,"DSM_SUN_DISTANCE_DEG_D",&sundistance);

	if(sunDistance(az_disp,el_disp,sunaz,sunel)<=SUNLIMIT) {

		strcpy(lastCommand,"Standby - Sun Limit (cmd)");
		SendLastCommandToDSM(lastCommand);
            	strcpy(messg, " Standing by. Sun Limit (cmd)");
		SendMessageToDSM(messg);
	 } /* sun limit check  with commanded position */
	
#endif

	az_disp_rm = az_disp / radian;
	el_disp_rm = el_disp / radian;
	dsm_status=dsm_write(DSM_HOST,"DSM_CMDDISP_AZ_DEG_D",&az_disp_rm);
	dsm_status=dsm_write(DSM_HOST,"DSM_CMDDISP_EL_DEG_D",&el_disp_rm);
/*
        redisWriteDouble("gltTrackComp","cmdAz",az_disp_rm);
        redisWriteDouble("gltTrackComp","cmdEl",el_disp_rm);
*/


	hour_angle = hangle2 * 12.0 / pi;

	hour_angle = hour_angle * 100.;
	hangleint = (int) hour_angle;

        /* if an offset has been commanded, then add it also
        to the previous values of az and el so that the
        computed rate does not have a jump in it */

        if(az_offset_flag==0) {
		azrate = az - az1;
/* added these following two lines on 23 sep 2002 */
		if(azrate < -6.0) azrate += 2.0* pi;
		if(azrate > 6.0) azrate -= 2.0* pi;
		}

        if(az_offset_flag==1) {
        azrate = prev_azrate;
        az_offset_flag=0;
        }

        if(el_offset_flag==0) elrate = el - el1;
        if(el_offset_flag==1) {
        elrate = prev_elrate;
        el_offset_flag=0;
        }                                         

	prev_azrate = azrate;
        prev_elrate = elrate;
                                 
	/* to detect the transit for a northern source */



	azint = az * milliarcsec;
	elint = el * milliarcsec;
	azrateint = azrate * milliarcsec;
	elrateint = elrate * milliarcsec;
	tjdint = (tjd - tjd0) * 24.0 * 3600.0 * 1000.0;
	az1 = az;
	el1 = el;
	hangle1 = hangle2;


	/*
	 * converting cmd az,el and rates to counts, and adjusting these for
	 * the azimuth lap ambiguity
	 */


	Az_cmd = (double) azint;
	az_cmd = (int) Az_cmd;	

	El_cmd = (double) elint;
	el_cmd = (int) El_cmd;

	Az_cmd_rate = (double) azrateint;

	El_cmd_rate = (double) elrateint;

#if SERVO
	/* check if servo is running */
	if((tsshm->msec)==checkmsec) {
	strcpy(messg,"servo is not running.");
	}
#endif

#if SERVO
	az_enc = tsshm->encAz;
        el_enc = tsshm->encEl;   
#else
	az_enc = 90.0;
	el_enc = 20.0;
#endif
/*
	az_actual=(double)az_enc/MSEC_PER_DEG;
        el_actual=(double)el_enc/MSEC_PER_DEG;
*/

        az_actual=az_enc_from_acu;
        el_actual=el_enc_from_acu;

                                                  

	el_actual_disp = el_actual-pmdel/3600.;
	az_actual_disp = az_actual-pmdaz/3600./cos(el_actual_disp*radian);

	az_actual_msec = az_actual_disp * 3600000.;
	el_actual_msec = el_actual_disp * 3600000.;


	az_actual_msec_int = (unsigned long) az_actual_msec;
	el_actual_msec_int = (unsigned long) el_actual_msec;


/* for GLT, the cable wrap logic needs different parameters:
Azimuth cable wrap neutral point is 0 deg instead of 90 deg as in SMA.
Azimuth minus end is at -270 deg instead of -180 deg for SMA.
Azimuth plus end is at +270 deg instead of +360 deg for SMA.
Allowing 10 deg for other limits, and hardcoding the numbers since 
servo doest not yet have the cwand ccwLimit values in tsshm.*/
/* 22 June 2012*/

/* note added on 11 nov 2017, at Thule, - now we have the az cable
wrap consistent with SMA's (-180 to 360 deg), so reverting back 
to previous logic */

/* This segment is from Socorro version
if (Az_cmd>260.*MSEC_PER_DEG)
                        {Az_cmd -= (360.*MSEC_PER_DEG);
                        az_disp-=(2*pi);
                        }
if(Az_cmd < -260.*MSEC_PER_DEG) 
			{Az_cmd += 360.*MSEC_PER_DEG;
                        if(az_disp<0.) az_disp+=2*pi;
			}
*/
if (Az_cmd>AZ_CW_LIMIT*MSEC_PER_DEG)
                        {Az_cmd -= (360.*MSEC_PER_DEG);
                        az_disp-=(2*pi);
                        }
if(Az_cmd < AZ_CCW_LIMIT*MSEC_PER_DEG) 
			{Az_cmd += 360.*MSEC_PER_DEG;
                        if(az_disp<0.) az_disp+=2*pi;
			}

if ((Az_cmd>=180.*MSEC_PER_DEG)&&
    (Az_cmd<=AZ_CW_LIMIT)&&(az_actual < 90.)) {
     Az_cmd -= (360.*MSEC_PER_DEG);
     az_disp-=(2.0*pi); 
     }
if (Az_cmd < AZ_CCW_LIMIT) {
    Az_cmd += 360.*MSEC_PER_DEG;
    if (az_disp<0.) az_disp += 2.0*pi;
    }
    


/* getting az and el tracking errors from shared memory */
/* in mas */

#if SERVO
        az_error = (tsshm->azTrError)/MSEC_PER_DEG;
        el_error = (tsshm->elTrError)/MSEC_PER_DEG;
#else
	az_error = 0.0;
	el_error = 0.0;
#endif
                                                         

/* correct the azimuth tracking error for cosine elevation */
	az_error = az_error * cos(el);

	posn_error = pow(((double) az_error * (double) az_error + (double) el_error * (double) el_error), 0.5);

	/* pass tracking errors through reflective memory */
	
        az_tracking_error=(float)az_error *3600.;
        el_tracking_error=(float)el_error *3600.;

/*
printf("azerror=%f elerror=%f\n",az_tracking_error,el_tracking_error);
*/


/*********** compute the running average of tracking errors (c. katz)*****/

#if 0

	tracking_error = sqrt((az_error * 3600.) * (az_error * 3600.)
			      + (el_error * 3600.) * (el_error * 3600.));
	tracking_error_accum -= *dp;
	tracking_error_accum += tracking_error;
	*dp = tracking_error;
	dp++;
	if ((dp - tracking_error_buffer) > (SMOOTH_LENGTH - 1))
	    dp = tracking_error_buffer;
	smoothed_tracking_error = tracking_error_accum / SMOOTH_LENGTH;
   /* write smoothed tracking error to DSM */ 


	if ((smoothed_tracking_error<18000.)&&(smoothed_tracking_error > 10.))
	{
	if(beep_flag==1) printf("");
	}
	
	if(smoothed_tracking_error>=10) waitflag=1;
	else waitflag=0;


/* uncomment this when we are ready for single-dish autocorrelation 
spectroscopy in position switching mode */

/***************************spectrometer part****************************/
	/* Pass flag to Charlie's spectrometer program for tracking_OK */

	if (smoothed_tracking_error > 10.)
	    source_on_off_flag = INVALID;

	else if (previous_source_on_off_flag == INVALID)

	{

	    if (on_source == 1)
	    {
		source_on_off_flag = ONSOURCE;
		on_source_timer = integration * 10;
	    }
	    if (off_source == 1)
	    {
		source_on_off_flag = OFFSOURCE;
		off_source_timer = integration * 10;
	    }
	}

	/*
	 * if Charlie's program is alive, send an interrupt to inform about
	 * the above flag changes
	 */
	/* first check if the spectrometer program is alive */
	dsm_status=dsm_read(DSM_HOST,"DSM_SPECTROMETER_ANTENNA_S", 
			&spectrometer,&timeStamp);
	/* if it is, then send interrupt */
	if (previous_source_on_off_flag != source_on_off_flag)
	{
	dsm_status=dsm_write_notify(DSM_HOST,"DSM_BLANKING_SOURCE_S",
				&source_on_off_flag);
	    send_spectrometer_interrupt = 0;
	}
	previous_source_on_off_flag = source_on_off_flag;
#endif

/******************************end of spectrometer part********************/

/************************* motion control ****************************/
/*
 * If scbComm = 1, then command motion through SCB else just
 * display the calculated and monitored values
 */

#if SERVO

        if (scbComm == 1)
        {
                /* load the position and rate and turn on the drives */

                if(servoOnFlag==0)
                {
                tsshm->elCmd = ON_CMD;                    
                tsshm->azCmd = ON_CMD;
 		servoOnFlag=1;
                }

	if((azelCommandFlag==1)&&(azscan_flag==0))
	{
	Az_cmd_rate=0.;
	}
	if((azelCommandFlag==1)&&(elscan_flag==0))
	{
	El_cmd_rate=0.;
	}

	servomilliseconds=tsshm->msec;

#if 0
	/* check for bad values of position*/
	if(Az_cmd < (tsshm->ccwLimit)) Az_cmd+=(360.*MSEC_PER_DEG);
	if(Az_cmd > (tsshm->cwLimit)) Az_cmd-=(360.*MSEC_PER_DEG);
#endif

                tsshm->az = Az_cmd;
                tsshm->azVel = Az_cmd_rate; 
/* added on 18 apr 2003 */
if(sun_avoid_flag==1) {
	if(suneloff>0.) El_cmd=HIGH_EL_TO_AVOID_SUN_MAS;
	if(suneloff<0.) El_cmd=LOW_EL_TO_AVOID_SUN_MAS;
	El_cmd_rate=0.;
}
                tsshm->el = El_cmd;
                tsshm->elVel = El_cmd_rate;
        tsshm->msecCmd = milliseconds;
        } /* if scbcomm=1 */
#endif
        if (scbComm==1) {
        if(target_flag==1) {
              respACU = ACUAzEl(Az_cmd/MSEC_PER_DEG,El_cmd/MSEC_PER_DEG);
                if(acuCurrentMode!=3) {respACU=ACUmode(3); acuCurrentMode=3;}
/*
                if(acuCurrentMode==5) respACU=ACUmode(5);
*/
              }
        if(target_flag==0) {/* tracking an astronomical source */

           if(firstTrack==1) {
           /* pass the full set of numPos values of az and el with timestamp */
        respACU=ACUprogTrack(NUMPOS,timeOfDay,azProgTrack, elProgTrack,0); 
        /*debug*/
        /* usleep(1000000); */
        /*usleep(200000);*/
           } else {
        respACU=ACUprogTrack(1, timeOfDay, azProgTrack, elProgTrack,0); 
        /*debug*/
        /* usleep(1000000); */
        /*usleep(200000);*/
           /* pass the (numPos+1)th set of values */
           }
           if(acuCurrentMode!=4) {
               respACU=ACUmode(4);
               acuCurrentMode=4;
               }
        }
        }
       if(firstTrack==1) firstTrack=0;
/****************************end of motion control*********************/

   
	/* displaying everything */

	if (icount == 1)
	    initflag = 0;
	if (icount != 1)
	    initflag = 1;

	azoff_int=(short)azoff;
	eloff_int=(short)eloff;
	dsm_status=dsm_write(DSM_HOST,"DSM_AZOFF_ARCSEC_D",&azoff);
	dsm_status=dsm_write(DSM_HOST,"DSM_ELOFF_ARCSEC_D",&eloff);
/*
        redisWriteDouble("gltTrackUser","azoff",azoff);
        redisWriteDouble("gltTrackUser","eloff",eloff);
*/




	switch (user)
	{
	case 'q':
	strcpy(lastCommand,"Exit from Track");
	SendLastCommandToDSM(lastCommand);



#if SERVO
          if (scbComm == 1)
            {
                tsshm->azCmd = OFF_CMD;
                tsshm->elCmd = OFF_CMD;
		scbComm = 0;
            }                   
#endif

          if (scbComm ==1) {
          /* put ACU in stop mode */
          ACUmode(1);
          acuCurrentMode=1;
          sleep(1);
          close(sockfdControl);
          close(sockfdMonitor);
          dsm_close();

          }

/*
          	dsm_status=dsm_clear_monitor();
                if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsmm_clear()");
                exit(1);
                }
*/
	    fprintf(stderr,"\nReceived signal: %d. Exiting track. Bye.\n",
			receivedSignal);
           ACUmode(1);
           acuCurrentMode=1;
/*
	    pthread_detach(CommandHandlerTID);
	    pthread_detach(ACUstatusTID);
*/
           close(sockfdControl);
           close(sockfdMonitor);
            dsm_close();
            freeReplyObject(redisResp);
            redisFree(redisC);
	    if (receivedSignal==SIGINT) exit(0);
	    if (receivedSignal==SIGTERM) exit(-1);
		user = -1;
	    break;

	case '0':
	strcpy(lastCommand,"Reset offsets and stop scans          ");
	SendLastCommandToDSM(lastCommand);
	    azscan_flag = 0;
	    elscan_flag = 0;
	    azoff = 0.;
	    eloff = 0.;
	    az_offset_flag=1;
            el_offset_flag=1;     
	    strcpy(messg, "                                       ");
		SendMessageToDSM(messg);
		user = -1;
	    break;

	case 'X':
	strcpy(lastCommand,"Load the new pointing model           ");
        SendLastCommandToDSM(lastCommand);
            azscan_flag = 0;
            elscan_flag = 0;
            azoff = 0.;
            eloff = 0.;
            az_offset_flag=1;
            el_offset_flag=1;
            strcpy(messg, "Loading new pointing model             ");
                SendMessageToDSM(messg);
	   read_mount_model_flag=1; 
                user = -1;
	   goto beginning;
	break;

	case 'O':
	strcpy(lastCommand,"Azoff commanded                       ");
	SendLastCommandToDSM(lastCommand);
        dsm_status=dsm_read(DSM_HOST,"DSM_COMMANDED_AZOFF_ARCSEC_D",&azoff,&timeStamp);
          if (dsm_status != DSM_SUCCESS) {
          dsm_error_message(dsm_status,"dsm_read(DSM_COMMANDED_AZOFF_ARCSEC_D)");
          }
	dsm_status=dsm_write(DSM_HOST,"DSM_AZOFF_ARCSEC_D",&azoff);
          if (dsm_status != DSM_SUCCESS) {
          dsm_error_message(dsm_status,"dsm_read(DSM_AZOFF_ARCSEC_D)");
          }
/*
        redisWriteDouble("gltTrackuser","azoff",azoff);
*/
	az_offset_flag=1;
		user = -1;
	break;

/* ignore command '5', which is handled by the encoder-server
for holography mapping */

	case '5':
	radio_flag=1;
		user = -1;
	break;

	case '4':
	radio_flag=0;
		user = -1;
	break;

	case 'P':
	strcpy(lastCommand,"Eloff commanded                       ");
	SendLastCommandToDSM(lastCommand);
        dsm_status=dsm_read(DSM_HOST,"DSM_COMMANDED_ELOFF_ARCSEC_D",&eloff,&timeStamp);
	dsm_status=dsm_write(DSM_HOST,"DSM_ELOFF_ARCSEC_D",&eloff);
/*
        redisWriteDouble("gltTrackuser","eloff",eloff);
*/
        el_offset_flag=1;
		user = -1;
	break;

	case 'T':
	strcpy(lastCommand,"Az/El commanded                       ");
	SendLastCommandToDSM(lastCommand);
        dsm_status=dsm_read(DSM_HOST,"DSM_COMMANDED_AZ_DEG_D",&commanded_az,&timeStamp);
        dsm_status=dsm_read(DSM_HOST,"DSM_COMMANDED_EL_DEG_D",&commanded_el,&timeStamp);
	azelCommandFlag=1;
        printf("got az/el: %f %f\n",commanded_az,commanded_el);
        respACU = ACUAzEl(commanded_az,commanded_el);
        if(acuCurrentMode!=3) {
        respACU=ACUmode(3);
        acuCurrentMode=3;
        }

		icount=0;
		interrupt_command_flag=0;
		target_flag = 1;
		user = -1;
		goto new_source;
	break;

	case 'S':
	strcpy(lastCommand,"Az/El rate commanded                       ");
	SendLastCommandToDSM(lastCommand);
        dsm_status=dsm_read(DSM_HOST,"DSM_CMD_AZRATE_D",&commanded_az_rate,&timeStamp);
        dsm_status=dsm_read(DSM_HOST,"DSM_CMD_ELRATE_D",&commanded_el_rate,&timeStamp);
            respACU = ACUAzElRate(commanded_az_rate,commanded_el_rate);
             if(acuCurrentMode!=5) {
             respACU=ACUmode(5);
             acuCurrentMode=5;
             }
		icount=0;
		interrupt_command_flag=0;
		target_flag = 1;
		user = -1;
                goto new_source;
	break;

	case 'U':
	strcpy(lastCommand,"Offset unit commanded                 ");
	SendLastCommandToDSM(lastCommand);
        dsm_status=dsm_read(DSM_HOST,"DSM_OFFSET_UNIT_ARCSEC_S",&scan_unit_int,&timeStamp);
	scan_unit=(double)scan_unit_int;
		user = -1;
	break;

	case 'i':
	strcpy(lastCommand,"Integration time set                  ");
	SendLastCommandToDSM(lastCommand);
        dsm_status=dsm_read(DSM_HOST,"DSM_INTEGRATION_TIME_SEC_S",&integration_short,&timeStamp);
	integration=(int)integration_short;
		user = -1;
	break;

	case 'p':
	strcpy(lastCommand,"Start position switching              ");
	SendLastCommandToDSM(lastCommand);
	    position_switching_flag = 1;
	    off_source_timer = integration;
	    on_source_timer = integration ;
	    on_source = 1;
	    off_source = 0;
		user = -1;
	    break;

	case 'a':
	strcpy(lastCommand,"Azimuth scan                          ");
	SendLastCommandToDSM(lastCommand);
	    azscan_flag = 1;
	    strcpy(messg, "                                       ");
		SendMessageToDSM(messg);
	    strcpy(messg, "azimuth scan");
		SendMessageToDSM(messg);
		user = -1;
	    break;

	case 'z':
	strcpy(lastCommand,"Stop az and el scans                  ");
	SendLastCommandToDSM(lastCommand);
	    azscan_flag = 0;
	    elscan_flag = 0;
		user = -1;
	    break;

	case 'e':
	strcpy(lastCommand,"Elevation scan                        ");
	SendLastCommandToDSM(lastCommand);
	    elscan_flag = 1;
	    strcpy(messg, "                                       ");
		SendMessageToDSM(messg);
	    strcpy(messg, "elevation scan");
		SendMessageToDSM(messg);
		user = -1;
	    break;

/* 7 and 9 are used for stow and unstow commands, redefine later
if needed for chopper beam offsets */
/*
	case '7':
	strcpy(lastCommand,"Azoff by minus chopper beam           ");
	SendLastCommandToDSM(lastCommand);
	    azoff = azoff - CHOPPER_BEAM ;
		user = -1;
	    break;

	case '9':
	strcpy(lastCommand,"Azoff by plus chopper beam            ");
	SendLastCommandToDSM(lastCommand);
	    azoff = azoff + CHOPPER_BEAM ;
		user = -1;
	    break;
*/

	case 'r':
	case 'R':
	strcpy(lastCommand,"Reset azoff and eloff                 ");
	SendLastCommandToDSM(lastCommand);
	    azoff = 0.0;
	    eloff = 0.0;
	    az_offset_flag=1;
	    el_offset_flag=1;
		user = -1;
	    break;


	/* adding a command for ra-dec offsets */
	case '1':
	strcpy(lastCommand,"Adding ra/dec offset                  ");
	SendLastCommandToDSM(lastCommand);
	dsm_status=dsm_read(DSM_HOST,"DSM_RAOFF_ARCSEC_D",&raOffset,&timeStamp);
	dsm_status=dsm_read(DSM_HOST,"DSM_DECOFF_ARCSEC_D",&decOffset,&timeStamp);
/*
        redisWriteDouble("gltTrackuser","raoff",raOffset);
        redisWriteDouble("gltTrackuser","decoff",decOffset);
*/
		icount=0;
		if(target_flag==1) target_flag=0;
		interrupt_command_flag=0;
	        if(errorflag==ERROR) errorflag=OK;
		radec_offset_flag=1;
		user = -1;
		goto new_source;

	    break;
	
	case 'n':
	az_offset_flag=1;
	el_offset_flag=1;
	strcpy(lastCommand,"Change source                         ");
	SendLastCommandToDSM(lastCommand);
	dsm_status=dsm_read(DSM_HOST,"DSM_SOURCE_LENGTH_S",&slength,&timeStamp);

	dsm_status=dsm_read(DSM_HOST,"DSM_SOURCE_C34", sname,&timeStamp);
 	 if (dsm_status != DSM_SUCCESS) {
         dsm_error_message(dsm_status,"dsm_read(DSM_SOURCE_C34)");
         }

	azelCommandFlag=0;

	dsm_status=dsm_read(DSM_HOST,"DSM_CMD_SOURCE_FLAG_L", &newSourceFlag,&timeStamp);
 	 if (dsm_status != DSM_SUCCESS) {
         dsm_error_message(dsm_status,"dsm_read(DSM_CMD_SOURCE_FLAG_L)");
         }

printf("got new source: %s %d\n",sname,newSourceFlag);

	if(newSourceFlag==1) sol_sys_flag=0;

		icount=0;
		if(target_flag==1) target_flag=0;
		interrupt_command_flag=0;
	        if(errorflag==ERROR) errorflag=OK;
		user = -1;
		goto new_source;

	    break;

	case '@':
	strcpy(lastCommand,"Standby - put track in simulation mode");
	SendLastCommandToDSM(lastCommand);
	
/*
	    if(scbComm==1)
            {
*/
            position_switching_flag = 0;
            strcpy(messg, "Standing by.                           ");
            respACU=ACUmode(1);
            acuCurrentMode=1;
#if SERVO
            tsshm->azCmd = OFF_CMD;
            tsshm->elCmd = OFF_CMD;
#endif
            scbComm=0;
	    servoOnFlag=0;
/*
            }            
*/

		user = -1;
	    break;

	case '!':
	strcpy(lastCommand,"Resume - put track in real mode       ");
	SendLastCommandToDSM(lastCommand);
	    if(scbComm==0)
	    {
		icount=0;
		scbComm = 1;
            strcpy(messg, " Resuming 				  ");
            /* if target mode, set Preset mode; if tracking mode 
               set Program Track mode */
            if(target_flag==1) {
            if (acuCurrentMode!=3) {respACU=ACUmode(3); acuCurrentMode=3;}
            }
            if(target_flag==0) {respACU=ACUmode(4); acuCurrentMode=4;}
		SendMessageToDSM(messg);
		user = -1;
		goto beginning;
	    }
	    break;

	case ';':
	if(beep_flag==1) {
	beep_flag=0;
	strcpy(lastCommand,"Turning off beeping");
	SendLastCommandToDSM(lastCommand);
		user = -1;
	break;
	}
	if(beep_flag==0) {
	beep_flag=1;
	strcpy(lastCommand,"Turning on beeping");
	SendLastCommandToDSM(lastCommand);
	}
		user = -1;
	break;


        case '7':
	strcpy(lastCommand,"Survival Stow");
	SendLastCommandToDSM(lastCommand);
        strcpy(messg, "Survival stow.                         ");
        respACU=ACUmode(7);
	user = -1;
        break;

        case '8':
	strcpy(lastCommand,"Maintenance Stow");
	SendLastCommandToDSM(lastCommand);
        strcpy(messg, "Maintenance stow.                      ");
        respACU=ACUmode(8);
	user = -1;
        break;

        case '9':
	strcpy(lastCommand,"Unstow");
	SendLastCommandToDSM(lastCommand);
        strcpy(messg, "Unstow.                                ");
        respACU=ACUmode(9);
	user = -1;
        break;

        case '(':
	strcpy(lastCommand,"Shutter Open");
	SendLastCommandToDSM(lastCommand);
        strcpy(messg, "Shutter open.                           ");
        respACU=ACUmode(10);
	user = -1;
        break;

        case ')':
	strcpy(lastCommand,"Shutter Close");
	SendLastCommandToDSM(lastCommand);
        strcpy(messg, "Shutter close.                         ");
        respACU=ACUmode(11);
	user = -1;
        break;

        case '+':
	strcpy(lastCommand,"Self test request");
	SendLastCommandToDSM(lastCommand);
        strcpy(messg, "Self test request                      ");
        respACU=ACUmode(12);
	user = -1;
        break;
	    
	}			/* end of switch */

	interrupt_command_flag=0;


#if DEBUG
	printf("source=%s\n", sname);
	printf("lst=%lf\n", lst_disp);
	printf("utc=%lf\n", utc_disp);
	printf("tjd=%d\n", tjd_disp);
	printf("ra=%lf\n", ra_disp);
	printf("dec=%lf\n", dec_disp);
	printf("ra0=%lf\n", ra0);
	printf("dec0=%lf\n", dec0);
	printf("radot=%lf\n", radot);
	printf("decdot=%lf\n", decdot);
	printf("target_flag=%d\n", target_flag);
	printf("icount=%d\n", icount);
	printf("ra_cat=%lf\n", ra_cat_disp);
	printf("dec_cat=%lf\n", dec_cat_disp);
	printf("az_disp=%lf\n", az_disp);
	printf("el_disp=%lf\n", el_disp);
	printf("temperature=%f\n",temperature);
	printf("humidity=%f\n",humidity);
	printf("pressure=%f\n",pressure);
	printf("refraction=%f\n",refraction);
#endif


/* transfer hour-angle and declination through refl.mem. (RPC)*/

#if COORDINATES_SVC
	hourangle=lst_disp - ra_disp;

	dsm_status=dsm_write(DSM_HOST,"DSM_HOUR_ANGLE_HR_D", &hourangle);
	dsm_status=dsm_write(DSM_HOST,"DSM_RA_APP_HR_D", &ra_disp);
	dsm_status=dsm_write(DSM_HOST,"DSM_DEC_APP_DEG_D",&dec_disp);
	dsm_status=dsm_write(DSM_HOST,"DSM_UTC_HR_D", &utc_disp);

/*
        redisWriteDouble("gltTrackComp","hourAngle",hourangle);
        redisWriteDouble("gltTrackComp","raApp",ra_disp);
        redisWriteDouble("gltTrackComp","decApp",dec_disp);
        redisWriteDouble("gltTrackComp","UTCh",utc_disp);
*/

/* magnitude is actually velocity in this single dish version- in case
we are observing sources other than stars for optical pointing */
        if(sol_sys_flag==0) dummyDouble=sourceVelocity;
        if(sol_sys_flag==1) {
                dummyDouble=radialVelocity;
                magnitude=(float)radialVelocity;
                }
	dsm_status=dsm_write(DSM_HOST,"DSM_SVEL_KMPS_D",&dummyDouble);
/*
        redisWriteDouble("gltTrackUser","svelkms",radialVelocity);
*/
	if(sol_sys_flag==0) dummyshortint=0x1;
	if(sol_sys_flag==1) dummyshortint=0x2;
	dsm_status=dsm_write(DSM_HOST,"DSM_SVELTYPE_S",&dummyshortint);
/*
        redisWriteShort("gltTrackUser","sveltype",dummyshortint);
*/

	dummyDouble=latitude_degrees;
	dsm_status=dsm_write(DSM_HOST,"DSM_LATITUDE_DEG_D", &dummyDouble);
/*
        redisWriteDouble("gltTrackFile","latitude",latitude_degrees);
*/

	dummyDouble= longitude_degrees;
	dsm_status=dsm_write(DSM_HOST,"DSM_LONGITUDE_DEG_D",&dummyDouble);
/*
        redisWriteDouble("gltTrackFile","longitude",longitude_degrees);
*/

	drefraction=(double)refraction;
	dsm_status=dsm_write(DSM_HOST,"DSM_REFRACTION_ARCSEC_D",&drefraction);
/*
        redisWriteDouble("gltTrackComp","refraction",drefraction);
*/
	if((drefraction<0.0)||(drefraction>4000.)) {
	  strcpy(operatorErrorMessage, "Refraction correction failed.");
/*
	  sendOpMessage(OPMSG_WARNING, 10, 30, operatorErrorMessage);
*/
          printf("Error: %s\n",operatorErrorMessage);
	}

	/* pointing model in RM*/
	
	if(radio_flag==1) {
        dsm_status=dsm_write(DSM_HOST,"DSM_AZDC_ARCSEC_D", &razdc);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZCOLLIMATION_ARCSEC_D", &razcol);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZELAXISTILT_ARCSEC_D", &reltilt);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTSIN_ARCSEC_D", &raztilt_sin);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTCOS_ARCSEC_D", &raztilt_cos);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTSIN2_ARCSEC_D", &raztilt_sin2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTCOS2_ARCSEC_D", &raztilt_cos2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCSIN_ARCSEC_D", &razenc_sin);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCCOS_ARCSEC_D", &razenc_cos);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCSIN2_ARCSEC_D", &razenc_sin2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCCOS2_ARCSEC_D", &razenc_cos2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCSIN3_ARCSEC_D", &razenc_sin3);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCCOS3_ARCSEC_D", &razenc_cos3);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZRMS_ARCSEC_D", &razmodelrms);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELDC_ARCSEC_D", &reldc);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELSAG_ARCSEC_D", &relsag);
/*
        dsm_status=dsm_write(DSM_HOST,"DSM_ELSAGSIN_ARCSEC_D", &relsagsin);
*/
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTSIN_ARCSEC_D", &reaztilt_sin);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTCOS_ARCSEC_D", &reaztilt_cos);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTSIN2_ARCSEC_D", &reaztilt_sin2);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTCOS2_ARCSEC_D", &reaztilt_cos2);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELRMS_ARCSEC_D", &relmodelrms);
        dsm_status=dsm_write(DSM_HOST,"DSM_MODELDATE_C10",rmodeldate);
	}
	if(radio_flag==0) {
        dsm_status=dsm_write(DSM_HOST,"DSM_AZDC_ARCSEC_D", &azdc);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZCOLLIMATION_ARCSEC_D", &azcol);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZELAXISTILT_ARCSEC_D", &eltilt);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTSIN_ARCSEC_D", &aztilt_sin);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTCOS_ARCSEC_D", &aztilt_cos);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTSIN2_ARCSEC_D", &aztilt_sin2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZAZTILTCOS2_ARCSEC_D", &aztilt_cos2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCSIN_ARCSEC_D", &azenc_sin);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCCOS_ARCSEC_D", &azenc_cos);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCSIN2_ARCSEC_D", &azenc_sin2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCCOS2_ARCSEC_D", &azenc_cos2);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCSIN3_ARCSEC_D", &azenc_sin3);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZENCCOS3_ARCSEC_D", &azenc_cos3);
        dsm_status=dsm_write(DSM_HOST,"DSM_AZRMS_ARCSEC_D", &azmodelrms);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELDC_ARCSEC_D", &eldc);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELSAG_ARCSEC_D", &elsag);
/*
        dsm_status=dsm_write(DSM_HOST,"DSM_ELSAGSIN_ARCSEC_D", &elsagsin);
*/
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTSIN_ARCSEC_D", &eaztilt_sin);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTCOS_ARCSEC_D", &eaztilt_cos);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTSIN2_ARCSEC_D", &eaztilt_sin2);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELAZTILTCOS2_ARCSEC_D", &eaztilt_cos2);
        dsm_status=dsm_write(DSM_HOST,"DSM_ELRMS_ARCSEC_D", &elmodelrms);
        dsm_status=dsm_write(DSM_HOST,"DSM_MODELDATE_C10",modeldate);
	}

#endif

	az_actual_corrected=az_actual_disp;
	
	if(sol_sys_flag==1)  planetdistance=distance;
	if(sol_sys_flag==0) {
	planetdistance=0.0;
	planetdiameter=0.0;
	}

        dsm_status=dsm_write(DSM_HOST,"DSM_PLANET_DISTANCE_AU_D", &planetdistance);
        dsm_status=dsm_write(DSM_HOST,"DSM_PLANET_DIAMETER_ARCSEC_D",&planetdiameter);

	
/* communicate some of the above variables to others through reflective
memory for monitoring purposes (azoff and eloff already written out
to RM ealier */



	 dummyByte=(char)radio_flag;
	dsm_status=dsm_write(DSM_HOST,"DSM_REFRACTION_RADIO_FLAG_B",&dummyByte);
/*
        redisWriteInt("gltTrackComp","refractionRadioFlag",radio_flag);
*/

/*
	    ret = dsm_status=dsm_write(DSM_HOST,"DSM_SOURCE_C34",sname);
*/

	lst_disp_float=(float)lst_disp;
	utc_disp_float=(float)utc_disp;
	dsm_status=dsm_write(DSM_HOST,"DSM_LST_HOURS_F",&lst_disp_float);
/*
        redisWriteFloat("gltTrackComp","lst",lst_disp_float);
*/
	dsm_status=dsm_write(DSM_HOST,"DSM_UTC_HOURS_F",&utc_disp_float);

	dsm_status=dsm_write(DSM_HOST,"DSM_TJD_D",&tjd_disp);
/*
        redisWriteDouble("gltTrackComp","tjd",tjd_disp);
*/

	dummyFloat=(float)ra_cat_disp;
	dsm_status=dsm_write(DSM_HOST,"DSM_RA_CAT_HOURS_F",&dummyFloat);
/*
        redisWriteFloat("gltTrackComp","raCat",dummyFloat);
*/
	dummyFloat=(float)dec_cat_disp;
	dsm_status=dsm_write(DSM_HOST,"DSM_DEC_CAT_DEG_F",&dummyFloat);
/*
        redisWriteFloat("gltTrackComp","decCat",dummyFloat);
*/

	dsm_status=dsm_write(DSM_HOST,"DSM_EPOCH_F",&epoch);
/*
        redisWriteFloat("gltTrackUser","epoch",epoch);
*/

	dsm_status=dsm_write(DSM_HOST,"DSM_ACTUAL_AZ_DEG_D",&az_actual_corrected);
	dsm_status=dsm_write(DSM_HOST,"DSM_ACTUAL_EL_DEG_D",&el_actual_disp);
/*
        redisWriteDouble("gltTrackComp","actualAz",az_actual_corrected);
        redisWriteDouble("gltTrackComp","actualEl",el_actual_disp);
*/

     if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_write() dsm_actual_az/el");
                }

	dsm_status=dsm_write(DSM_HOST,"DSM_PMDAZ_F",&pmdaz);
	dsm_status=dsm_write(DSM_HOST,"DSM_PMDEL_F",&pmdel);
/*
        redisWriteFloat("gltTrackComp","pmdaz",pmdaz);
        redisWriteFloat("gltTrackComp","pmdel",pmdel);
*/

	fflush(stdout);

#if SERVO
	checkmsec=tsshm->msec; /* for checking if servo is running*/
#endif


  /* timestamp for DERS etc..*/
        dsm_status=dsm_read(DSM_HOST,"DSM_UNIX_TIME_L",&timestamp,&timeStamp);
/*
        redisWriteInt("gltTrackComp","unixTime",timeStamp);
*/
        dsm_status=dsm_write(DSM_HOST,"DSM_TRACK_TIMESTAMP_L",&timestamp);
/*
        redisWriteInt("gltTrackComp","trackTimestamp",timestamp);
*/
/*
printf("%d\n",timestamp);
*/

	usleep(loop_time);
    }				/* this is the big while loop */

return(0);
}				/* end of main */

void
split(unsigned long * lw, unsigned short * sw1, unsigned short * sw2)
{
    *sw1 = *lw & 0xFFFF;
    *sw2 = (*lw & 0xFFF0000) / 0x10000;
}


/*------------name.c--------*/
void
print_upper(char *name)
{
    register int    t;

    for (t = 0; name[t]; ++t)
    {
	name[t] = tolower(name[t]);
	/*
	 * putchar (name[t]);
	 */

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
	*id = 4;
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
    } else if (!strcmp(s, "comet_a3            "))
    {
  	*id = 385;
	*flag = 1;
    } else if (!strcmp(s, "2005yu55            "))
    {
  	*id = 386;
	*flag = 1;
    } else
    {
	*id = 0;
	*flag = 0;
    }
}

/* The following function does the standard star catalog look up */

void
starcat(char *s, int *star_flag, struct source * observe_source)
{

    FILE           *fp2;

    int             end_of_file=0;

    int             number;

    int             rah, ram, decd, decm;

    float           vel, epoch, ras, decs, pmr, pmd;

    char            vtype[20], source_name[20], comment[100];
    char            decsign;

    *star_flag = 0;

/*
bug: 3 aug 2009: 1st source is skipped
*/
	if(radio_flag==1) {
	fp2 = fopen("/global/catalogs/sma_catalog", "r");
        end_of_file = fscanf(fp2, 
	"%s %d %d %f %c%2d %d %f %f %f %f %s %f %s",
	 source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
		    &pmr, &pmd, &epoch, vtype, &magnitude, comment);
	}

    	if(radio_flag==0) {
	fp2 = fopen("/global/catalogs/sma_optical_catalog", "r");
        end_of_file = fscanf(fp2, 
	"%d %s %d %d %f %c%2d %d %f %f %f %f %s %f %s %s",
	 &number,source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
		    &pmr, &pmd, &epoch, vtype, &magnitude, comment,sptype);
	}

	
        while (end_of_file != EOF) {

	if(radio_flag==1) {
        end_of_file = fscanf(fp2, 
	"%s %d %d %f %c%2d %d %f %f %f %f %s %f %s",
	 source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
		    &pmr, &pmd, &epoch, vtype, &magnitude, comment);
	}

    	if(radio_flag==0) {
        end_of_file = fscanf(fp2, 
	"%d %s %d %d %f %c%2d %d %f %f %f %f %s %f %s %s",
	 &number,source_name, &rah, &ram, &ras, &decsign, &decd, &decm, &decs,
		    &pmr, &pmd, &epoch, vtype, &magnitude, comment, sptype);
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
	observe_source->vel = magnitude;
	sourceVelocity=(double)magnitude;
	strcpy(observe_source->comment, comment);
	strcpy(messg, " ");
	break;
	}
	if(radio_flag==1) {
        if (end_of_file!=14) {
	strcpy(messg, "Source catalog is corrupted. ");
        SendMessageToDSM(messg);
	fprintf(stderr,"%s\n",messg);
	strcpy(operatorErrorMessage, "Source catalog is corrupted.");
/*
	  sendOpMessage(OPMSG_WARNING, 10, 30, operatorErrorMessage);
*/
          printf("Error: %s\n",operatorErrorMessage);
        break;
         }}
	if(radio_flag==0) {
        if (end_of_file!=16) {
	strcpy(messg, "Source catalog is corrupted. ");
        SendMessageToDSM(messg);
	fprintf(stderr,"%s\n",messg);
	strcpy(operatorErrorMessage, "Source catalog is corrupted.");
/*
	  sendOpMessage(OPMSG_WARNING, 10, 30, operatorErrorMessage);
*/
          printf("Error: %s\n",operatorErrorMessage);
        break;
         }}

        end_of_file=0;
    }
    fclose(fp2);
}

void SendMessageToDSM(char *messg)
{
int messagelength;
char blank[100];
messagelength=strlen(messg);
sprintf(blank,"                                                                                                   ");                      
dsm_status=dsm_write(DSM_HOST,"DSM_TRACK_MESSAGE_C100",blank);
dsm_status=dsm_write(DSM_HOST,"DSM_TRACK_MESSAGE_C100",messg);
/*
        redisWriteString("gltTrackComp","trackMsg",messg);
*/
}

void SendLastCommandToDSM(char *lastCommand)
{
int messagelength;
char blank[100];
messagelength=strlen(lastCommand);

sprintf(blank,"                                                                                                   ");                      
dsm_status=dsm_write(DSM_HOST,"DSM_TRACK_LAST_COMMAND_C100",blank);
dsm_status=dsm_write(DSM_HOST,"DSM_TRACK_LAST_COMMAND_C100",lastCommand);
/*
        redisWriteString("gltTrackComp","trackLastCmd",lastCommand);
*/
}

/* Functions to write data  to Redis */

void redisWriteShort(char *hashName, char *fieldName, short variable)
{
        sprintf(redisData,"HSET %s %s %h",hashName,fieldName,variable);
        redisResp = redisCommand(redisC,redisData);
}

void redisWriteInt(char *hashName, char *fieldName, int variable)
{
        sprintf(redisData,"HSET %s %s %d",hashName,fieldName,variable);
        redisResp = redisCommand(redisC,redisData);
}
void redisWriteFloat(char *hashName, char *fieldName,float variable)
{
        sprintf(redisData,"HSET %s %s %f",hashName,fieldName,variable);
        redisResp = redisCommand(redisC,redisData);
}
void redisWriteDouble(char *hashName, char *fieldName, double variable)
{
        sprintf(redisData,"HSET %s %s %lf",hashName,fieldName,variable);
        redisResp = redisCommand(redisC,redisData);
}
void redisWriteString(char *hashName, char *fieldName, char *variable)
{
        sprintf(redisData,"HSET %s %s %s",hashName,fieldName,variable);
        redisResp = redisCommand(redisC,redisData);
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

/* Interrupt handler for receiving commands from console command hrough DSM*/

void *CommandHandler() {
char command[30];
/*
char name[DSM_NAME_LENGTH];
int ant=DSM_HOST;
*/
time_t timeStamp;


	while(1)
	{
	

/*
Getting dsm_read_wait(): internal error -- contact maintainer error
replacing read_wait by read for now.
	dsm_status=dsm_read_wait("gltobscon","DSM_COMMAND_FLAG_S",&command_flag);
*/
	dsm_status=dsm_read("gltobscon","DSM_COMMAND_FLAG_S",&command_flag,&timeStamp);
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_read_wait() DSM_COMMAND_FLAG_S");
                exit(1);
/*
        fprintf(stderr,"Interrupt received. command_flag=%d\n",command_flag);
	fflush(stderr);
*/
	}
	
	if(command_flag==0) {
	dsm_status=dsm_read(DSM_HOST,"DSM_COMMANDED_TRACK_COMMAND_C30",command,&timeStamp);
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_read()");
                exit(1);
        }

        user=(int)command[0];
	interrupt_command_flag=1;

	command_flag=1;
	dsm_status=dsm_write("gltobscon","DSM_COMMAND_FLAG_S",&command_flag);
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_write()");
                exit(1);
	}
	
	} /* if command flag is zero, handle command and set it back to 1 */

        usleep(100000);

	} /* while */

	pthread_detach(CommandHandlerTID);
	pthread_exit((void *) 0);
}

void *ACUstatus() {

  int i,n = 0;
  char recvBuff[256];
  char sendBuff[256];
  int days,hh,mm;
  int dsm_status,dsm_open_status;
  double az=0.,el=0.;
  double acuCmdAz,acuCmdEl;
  double hours,minutes,seconds;
  float az_tracking_error,el_tracking_error;
  struct sockaddr_in serv_addr;
  acuStatus acuStatusResp;
  acuCmd acuCommand={};

  short acuModeAz,acuModeEl;
  char azServoStatus[2],elServoStatus[2];
  int acuDay,acuHour;
  char acuErrorMessage[256],acuSystemGS[6];
  short checksum;


  acuCommand.stx = 0x2;
  acuCommand.id = 0x71;
  acuCommand.datalength = htole16(0x7);
/*
  acuCommand.checksum = htole16(calculate_checksum(
                   (char *)&acuCommand.id, offsetof(acuCmd, checksum) - offsetof(acuCmd,id)));
*/
  acuCommand.etx = 0x3;

  checksum = checkSum((char*)(&acuCommand), sizeof(acuCommand));

   if(checksum > 0xffff) checksum=checksum & 0xffff;

   acuCommand.checksum = htole16(checksum);


  memset(recvBuff, '0' ,sizeof(recvBuff));
  memset(sendBuff, '0' ,sizeof(sendBuff));

/*
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0) {
      printf("\n Error : Could not create socket \n");
      return 1;
    }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(9110);
  serv_addr.sin_addr.s_addr = inet_addr("172.16.5.95");

  if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0) {
      printf("\n Error : Connect Failed \n");
      return 1;
    }
*/

  memcpy(sendBuff,(char*)&acuCommand,sizeof(acuCommand));


	while(1) {
	
  n = send(sockfdMonitor,sendBuff,sizeof(acuCommand),0);
  if (n<0) printf("ERROR writing to ACU.");

  /* receive the ACK response from ACU */
  n = recv(sockfdMonitor, recvBuff, sizeof(acuStatusResp),0);

  if( n < 0)  printf("\n Read Error \n");

  /* check if ACK is received, then receive the response and parse it */
  if (recvBuff[0]==0x6) {

  n = recv(sockfdMonitor, (char *)&acuStatusResp, sizeof(acuStatusResp),0);

/*
  printf("Read %d bytes from ACU\n",n);
  printf ("Received %d bytes from ACU.\n",acuStatusResp.datalength);
*/
/*
printf("Time: %d\n",acuStatusResp.timeOfDay);
*/
  hours = acuStatusResp.timeOfDay/3600000.;
  hh = (int)hours;
  minutes = (hours-hh)*60.;
  mm = (int) minutes;
  seconds = (minutes-mm)*60.;
/*
  printf ("ACU Time: (day, hh:mm:ss.sss):  %d %02d:%02d:%02.3f\n",acuStatusResp.dayOfYear,hh,mm,seconds);
*/
/*

  printf ("azPos (deg): %f \n",(double)acuStatusResp.azPos/1.0e6);
  printf ("elPos (deg): %f \n",(double)acuStatusResp.elPos/1.0e6);
  printf ("cmdAzPos (deg): %f \n",(double)acuStatusResp.cmdAzPos/1.0e6);
  printf ("cmdElPos (deg): %f \n",(double)acuStatusResp.cmdElPos/1.0e6);
  printf ("azStatusMode: 0x%x \n",acuStatusResp.azStatusMode);
  printf ("elStatusMode: 0x%x \n",acuStatusResp.elStatusMode);
  printf ("servoSystemStatusAz bytes 1,2: 0x%x 0x%x\n",acuStatusResp.servoSystemStatusAz[0],acuStatusResp.servoSystemStatusAz[1]);
*/

/* write az and el to dsm */
  az = (double)acuStatusResp.azPos/1.0e6;
  el = (double)acuStatusResp.elPos/1.0e6;
  acuCmdAz = (double)acuStatusResp.cmdAzPos/1.0e6;
  acuCmdEl = (double)acuStatusResp.cmdElPos/1.0e6;

/* adjusting the az encoder dc offset temporarily: 29 Jan 2018
  for testing */

/* az = az + 5.93125; */

/* Removed above offset after ACU change: 5 Mar 2018 */

  az_enc_from_acu=az;
  el_enc_from_acu=el; 

  dsm_status = dsm_write(ACC,"DSM_AZ_POSN_DEG_D",&az);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_AZ_POSN_DEG dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET acu azPosn %lf",az);
        redisResp = redisCommand(redisC,redisData);

  dsm_status = dsm_write(ACC,"DSM_EL_POSN_DEG_D",&el);

  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_EL_POSN_DEG dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET acu elPosn %lf",el);
        redisResp = redisCommand(redisC,redisData);

  dsm_status = dsm_write(ACC,"DSM_AZ_ACU_CMD_POSN_DEG_D",&acuCmdAz);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_AZ_ACU_CMD_POSN_DEG dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET acu azCmdPosn %lf",acuCmdAz);
        redisResp = redisCommand(redisC,redisData);

  dsm_status = dsm_write(ACC,"DSM_EL_ACU_CMD_POSN_DEG_D",&acuCmdEl);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_EL_ACU_CMD_POSN_DEG dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET acu elCmdPosn %lf",acuCmdEl);
        redisResp = redisCommand(redisC,redisData);

  az_tracking_error = (float)(acuCmdAz-az)*3600.;
  el_tracking_error = (float)(acuCmdEl-el)*3600.;
/*
  az_tracking_error = 0.0;
  el_tracking_error = 0.0;
*/
  dsm_status = dsm_write(ACC,"DSM_AZ_TRACKING_ERROR_F",&az_tracking_error);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET gltTrackComp azTrackingError %f",az_tracking_error);
        redisResp = redisCommand(redisC,redisData);

  dsm_status = dsm_write(ACC,"DSM_EL_TRACKING_ERROR_F",&el_tracking_error);

  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET gltTrackComp elTrackingError %f",el_tracking_error);
        redisResp = redisCommand(redisC,redisData);

  acuModeAz=acuStatusResp.azStatusMode;
  acuModeEl=acuStatusResp.elStatusMode;
  strcpy(azServoStatus, acuStatusResp.servoSystemStatusAz);
  strcpy(elServoStatus, acuStatusResp.servoSystemStatusEl);
  acuHour=acuStatusResp.timeOfDay; /*msec*/
  acuDay=acuStatusResp.dayOfYear;
  
  for(i=0;i<6;i++) {
  acuSystemGS[i]=acuStatusResp.servoSystemGS[i];
  }

  dsm_status = dsm_write(ACC,"DSM_ACU_SERVO_STATUS_AZ_C2",azServoStatus);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_SERVO_STATUS_AZ_C2 dsm_status=%d\n",dsm_status);
  }
  dsm_status = dsm_write(ACC,"DSM_ACU_SERVO_STATUS_EL_C2",elServoStatus);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_SERVO_STATUS_EL_C2 dsm_status=%d\n",dsm_status);
  }
  dsm_status = dsm_write(ACC,"DSM_ACU_MODE_STATUS_AZ_S",&acuModeAz);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_MODE_STATUS_AZ_S dsm_status=%d\n",dsm_status);
  }
  dsm_status = dsm_write(ACC,"DSM_ACU_MODE_STATUS_EL_S",&acuModeEl);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_MODE_STATUS_EL_S dsm_status=%d\n",dsm_status);
  }
  dsm_status = dsm_write(ACC,"DSM_ACU_SYSTEMGS_C6",acuSystemGS);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_SYSTEMGS_C6 dsm_status=%d\n",dsm_status);
  }
  dsm_status = dsm_write(ACC,"DSM_ACU_DAYOFYEAR_L",&acuDay);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_DAYOFYEAR_L dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET acu dayOfYear %d",acuDay);
        redisResp = redisCommand(redisC,redisData);
  dsm_status = dsm_write(ACC,"DSM_ACU_HOUR_L",&acuHour);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_HOUR_L dsm_status=%d\n",dsm_status);
  }
        sprintf(redisData,"HSET acu hour %d",acuHour);
        redisResp = redisCommand(redisC,redisData);


/*
  if (acuStatusResp.azStatusMode == 0x1) printf(" Az stop.\n");
  if (acuStatusResp.azStatusMode == 0x21) printf(" Az Maintenance.\n");
  if (acuStatusResp.azStatusMode == 0x2) printf(" Az Preset.\n");
  if (acuStatusResp.azStatusMode == 0x3) printf(" Az Program Track.\n");
  if (acuStatusResp.azStatusMode == 0x4) printf(" Az Rate.\n");
  if (acuStatusResp.azStatusMode == 0x5) printf(" Az Sector Scan.\n");
  if (acuStatusResp.azStatusMode == 0x6) printf(" Az Survival Position (stow).\n");
  if (acuStatusResp.azStatusMode == 0xe) printf(" Az Maintenance Position (stow).\n");
  if (acuStatusResp.azStatusMode == 0x4e) printf(" Az Stow (stow pins not retracted).\n");
  if (acuStatusResp.azStatusMode == 0x26) printf(" Az unstow.\n");
  if (acuStatusResp.azStatusMode == 0x8) printf(" Az Two line track.\n");
  if (acuStatusResp.azStatusMode == 0x9) printf(" Az Star Track.\n");
  if (acuStatusResp.azStatusMode == 0x29) printf(" Az Sun Track.\n");

  if (acuStatusResp.elStatusMode == 1) printf(" El stop.\n");
  if (acuStatusResp.elStatusMode == 33) printf(" El Maintenance.\n");
  if (acuStatusResp.elStatusMode == 2) printf(" El Preset.\n");
  if (acuStatusResp.elStatusMode == 3) printf(" El Program Track.\n");
  if (acuStatusResp.elStatusMode == 4) printf(" El Rate.\n");
  if (acuStatusResp.elStatusMode == 5) printf(" El Sector Scan.\n");
  if (acuStatusResp.elStatusMode == 6) printf(" El Survival Position (stow).\n");
  if (acuStatusResp.elStatusMode == 14) printf(" El Maintenance Position (stow).\n");
  if (acuStatusResp.elStatusMode == 78) printf(" El Stow (stow pins not retracted).\n");
  if (acuStatusResp.elStatusMode == 38) printf(" El unstow.\n");
  if (acuStatusResp.elStatusMode == 8) printf(" El Two line track.\n");
  if (acuStatusResp.elStatusMode == 9) printf(" El Star Track.\n");
  if (acuStatusResp.elStatusMode == 41) printf(" El Sun Track.\n");

  if (acuStatusResp.servoSystemStatusAz[0] & 1)
                printf(" Az emergency limit.\n");
  if (acuStatusResp.servoSystemStatusAz[0] & 2)
                printf(" Az operating limit ccw.\n");
  if (acuStatusResp.servoSystemStatusAz[0] & 4)
                printf(" Az operating limit cw.\n");
  if (acuStatusResp.servoSystemStatusAz[0] & 8)
                printf(" Az prelimit ccw.\n");
  if (acuStatusResp.servoSystemStatusAz[0] & 16)
                printf(" Az prelimit cw.\n");
  if (acuStatusResp.servoSystemStatusAz[0] & 32)
                printf(" Az stow position.\n");
  if (acuStatusResp.servoSystemStatusAz[0] & 64)
                printf(" Az stow pin inserted.\n");
  if (acuStatusResp.servoSystemStatusAz[0] & 128)
                printf(" Az stow pin retracted.\n");

 if (acuStatusResp.servoSystemStatusAz[1] & 1)
                printf(" Az servo failure.\n");
  if (acuStatusResp.servoSystemStatusAz[1] & 2)
                printf(" Az brake failure.\n");
  if (acuStatusResp.servoSystemStatusAz[1] & 4)
                printf(" Az encoder failure.\n");
  if (acuStatusResp.servoSystemStatusAz[1] & 8)
                printf(" Az auxiliary mode.\n");
  if (acuStatusResp.servoSystemStatusAz[1] & 16)
                printf(" Az motion failure.\n");
  if (acuStatusResp.servoSystemStatusAz[1] & 32)
                printf(" Az CAN bus failure.\n");
  if (acuStatusResp.servoSystemStatusAz[1] & 64)
                printf(" Az axis disabled.\n");
  if (acuStatusResp.servoSystemStatusAz[1] & 128)
                printf(" Az computer disabled (local mode).\n");

  printf ("servoSystemStatusEl bytes 1,2: 0x%x 0x%x \n",acuStatusResp.servoSystemStatusEl[0],acuStatusResp.servoSystemStatusEl[1]);
  if (acuStatusResp.servoSystemStatusEl[0] & 1)
                printf(" El emergency limit.\n");
  if (acuStatusResp.servoSystemStatusEl[0] & 2)
                printf(" El operating limit ccw.\n");
  if (acuStatusResp.servoSystemStatusEl[0] & 4)
                printf(" El operating limit cw.\n");
  if (acuStatusResp.servoSystemStatusEl[0] & 8)
                printf(" El prelimit ccw.\n");
  if (acuStatusResp.servoSystemStatusEl[0] & 16)
                printf(" El prelimit cw.\n");
  if (acuStatusResp.servoSystemStatusEl[0] & 32)
                printf(" El stow position.\n");
  if (acuStatusResp.servoSystemStatusEl[0] & 64)
                printf(" El stow pin inserted.\n");
  if (acuStatusResp.servoSystemStatusEl[0] & 128)
                printf(" El stow pin retracted.\n");

  if (acuStatusResp.servoSystemStatusEl[1] & 1)
                printf(" El servo failure.\n");
  if (acuStatusResp.servoSystemStatusEl[1] & 2)
                printf(" El brake failure.\n");
  if (acuStatusResp.servoSystemStatusEl[1] & 4)
                printf(" El encoder failure.\n");
  if (acuStatusResp.servoSystemStatusEl[1] & 8)
                printf(" El auxiliary mode.\n");
  if (acuStatusResp.servoSystemStatusEl[1] & 16)
                printf(" El motion failure.\n");
  if (acuStatusResp.servoSystemStatusEl[1] & 32)
                printf(" El CAN bus failure.\n");
  if (acuStatusResp.servoSystemStatusEl[1] & 64)
                printf(" El axis disabled.\n");
  if (acuStatusResp.servoSystemStatusEl[1] & 128)
                printf(" El computer disabled (local mode).\n");

  printf ("servoSystemGStatus1-6: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",acuStatusResp.servoSystemGS[0],
  acuStatusResp.servoSystemGS[1],
  acuStatusResp.servoSystemGS[2],
  acuStatusResp.servoSystemGS[3],
  acuStatusResp.servoSystemGS[4],
  acuStatusResp.servoSystemGS[5]);
*/

/*
  if (acuStatusResp.servoSystemGS[0] & 1)
                printf(" Door Interlock.\n");
  if (acuStatusResp.servoSystemGS[0] & 2)
                printf(" SAFE .\n");
  if (acuStatusResp.servoSystemGS[0] & 64)
                printf(" Emergency off.\n");
  if (acuStatusResp.servoSystemGS[0] & 128)
                printf(" Not on source.\n");

  if (acuStatusResp.servoSystemGS[1] & 4)
                printf(" Time error.\n");
  if (acuStatusResp.servoSystemGS[1] & 8)
                printf(" Year error.\n");
  if (acuStatusResp.servoSystemGS[1] & 32)
                printf(" Green mode active.\n");
  if (acuStatusResp.servoSystemGS[1] & 64)
                printf(" Speed high.\n");
  if (acuStatusResp.servoSystemGS[1] & 128)
                printf(" Remote.\n");

  if (acuStatusResp.servoSystemGS[2] & 1)
                printf(" Spline green.\n");
  if (acuStatusResp.servoSystemGS[2] & 2)
                printf(" Spline yellow.\n");
  if (acuStatusResp.servoSystemGS[2] & 4)
                printf(" Spline red.\n");
  if (acuStatusResp.servoSystemGS[2] & 16)
                printf(" Gearbox oil level warning.\n");
  if (acuStatusResp.servoSystemGS[2] & 32)
                printf(" PLC interface ok.\n");

  if (acuStatusResp.servoSystemGS[3] & 1)
                printf(" PCU mode.\n");
  if (acuStatusResp.servoSystemGS[3] & 4)
                printf(" Tiltmeter error.\n");

  if (acuStatusResp.servoSystemGS[4] & 1)
                printf(" Cabinet overtemperature.\n");
  if (acuStatusResp.servoSystemGS[4] & 4)
                printf(" Shutter open.\n");
  if (acuStatusResp.servoSystemGS[4] & 8)
                printf(" Shutter closed.\n");
  if (acuStatusResp.servoSystemGS[4] & 16)
                printf(" Shutter failure.\n");
*/
  }

  if (recvBuff[0]==0x2) {
  sprintf(acuErrorMessage,"ACU refuses the command from ACUstatus...reason:");
  dsm_status = dsm_write(ACC,"DSM_ACU_ERROR_MESSAGE_C256",acuErrorMessage);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_ERROR_MESSAGE_C256 dsm_status=%d\n",dsm_status);
  }
  sleep(2);
  if (recvBuff[1]==0x43) sprintf(acuErrorMessage,"Checksum error.\n");
  if (recvBuff[1]==0x45) sprintf(acuErrorMessage,"ETX not received at expected position.\n");
  if (recvBuff[1]==0x49) sprintf(acuErrorMessage,"Unknown identifier.\n");
  if (recvBuff[1]==0x4C) sprintf(acuErrorMessage,"Wrong length (incorrect no. of bytes rcvd.\n");
  if (recvBuff[1]==0x6C) sprintf(acuErrorMessage,"Specified length does not match identifier.\n");
  if (recvBuff[1]==0x4D) sprintf(acuErrorMessage,"Command ignored in present operating mode.\n");
  if (recvBuff[1]==0x6F) sprintf(acuErrorMessage,"Other reasons.\n");
  if (recvBuff[1]==0x52) sprintf(acuErrorMessage,"Device not in Remote mode.\n");
  if (recvBuff[1]==0x72) sprintf(acuErrorMessage,"Value out of range.\n");
  if (recvBuff[1]==0x53) sprintf(acuErrorMessage,"Missing STX.\n");
  } else {sprintf(acuErrorMessage,"");}

  dsm_status = dsm_write(ACC,"DSM_ACU_ERROR_MESSAGE_C256",acuErrorMessage);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_ERROR_MESSAGE_C256 dsm_status=%d\n",dsm_status);
  }


        /* usleep(2000000); */
        usleep(100000);
	} /* while loop */

	pthread_detach(ACUstatusTID);
	pthread_exit((void *) 0);
}

void *ACUiostatus() {

  int i,n = 0;
  char recvBuff[256];
  char sendBuff[256];
  char acuErrorMessage[256];
  int days,hh,mm;
  int dsm_status,dsm_open_status;
  double hours,minutes,seconds;
  struct sockaddr_in serv_addr;
  ioStatus ioStatusResp;
  acuCmd acuCommand={};
  short checksum;

  short azmotor1temp;
  short azmotor2temp;
  short azmotor3temp;
  short azmotor4temp;
  short elmotor1temp;
  short elmotor2temp;
  short elmotor3temp;
  short elmotor4temp;
  short az1motorcurrent;
  short az2motorcurrent;
  short el1motorcurrent;
  short el2motorcurrent;
  short el3motorcurrent;
  short el4motorcurrent;
  float az1motorcurrentF;
  float az2motorcurrentF;
  float el1motorcurrentF;
  float el2motorcurrentF;
  float el3motorcurrentF;
  float el4motorcurrentF;


  acuCommand.stx = 0x2;
  acuCommand.id = 0x51;
  acuCommand.datalength = htole16(0x7);
/*
  acuCommand.checksum = htole16(calculate_checksum(
                   (char *)&acuCommand.id, offsetof(acuCmd, checksum) - offsetof(acuCmd,id)));
*/
  acuCommand.etx = 0x3;

   checksum = checkSum((char*)(&acuCommand), sizeof(acuCommand));

   if(checksum > 0xffff) checksum=checksum & 0xffff;

   acuCommand.checksum = htole16(checksum);



  memset(recvBuff, '0' ,sizeof(recvBuff));
  memset(sendBuff, '0' ,sizeof(sendBuff));

  memcpy(sendBuff,(char*)&acuCommand,sizeof(acuCommand));


	while(1) {
	
  n = send(sockfdMonitor,sendBuff,sizeof(acuCommand),0);
  if (n<0) printf("ERROR writing to ACU.");

  /* receive the ACK response from ACU */
  n = recv(sockfdMonitor, recvBuff, sizeof(ioStatusResp),0);

  if( n < 0)  printf("\n Read Error \n");

  /* check if ACK is received, then receive the response and parse it */
  if (recvBuff[0]==0x6) {

  n = recv(sockfdMonitor, (char *)&ioStatusResp, sizeof(ioStatusResp),0);

/*
  printf("Read %d bytes from ACU\n",n);
  printf ("Received %d bytes from ACU.\n",ioStatusResp.datalength);
*/
/*
printf("Time: %d\n",ioStatusResp.timeOfDay);
*/
  hours = ioStatusResp.timeOfDay/3600000.;
  hh = (int)hours;
  minutes = (hours-hh)*60.;
  mm = (int) minutes;
  seconds = (minutes-mm)*60.;
/*
  printf ("ACU Time: (day, hh:mm:ss.sss):  %d %02d:%02d:%02.3f\n",ioStatusResp.dayOfYear,hh,mm,seconds);
*/
/*

  printf ("azPos (deg): %f \n",(double)ioStatusResp.azPos/1.0e6);
  printf ("elPos (deg): %f \n",(double)ioStatusResp.elPos/1.0e6);
  printf ("cmdAzPos (deg): %f \n",(double)ioStatusResp.cmdAzPos/1.0e6);
  printf ("cmdElPos (deg): %f \n",(double)ioStatusResp.cmdElPos/1.0e6);
  printf ("azStatusMode: 0x%x \n",ioStatusResp.azStatusMode);
  printf ("elStatusMode: 0x%x \n",ioStatusResp.elStatusMode);
  printf ("servoSystemStatusAz bytes 1,2: 0x%x 0x%x\n",ioStatusResp.servoSystemStatusAz[0],ioStatusResp.servoSystemStatusAz[1]);
*/

  azmotor1temp = (short) ioStatusResp.ServoAmpAz1status[0]-100;
  azmotor2temp = (short) ioStatusResp.ServoAmpAz2status[0]-100;

  azmotor3temp = (short) ioStatusResp.ServoAmpAz3status[0]-100;
  azmotor4temp = (short) ioStatusResp.ServoAmpAz4status[0]-100;

  elmotor1temp = (short) ioStatusResp.ServoAmpEl1status[0]-100;
  elmotor2temp = (short) ioStatusResp.ServoAmpEl2status[0]-100;
  elmotor3temp = (short) ioStatusResp.ServoAmpEl3status[0]-100;
  elmotor4temp = (short) ioStatusResp.ServoAmpEl4status[0]-100;

  az1motorcurrent = (short) ioStatusResp.Az1motorCurrent;
  az2motorcurrent = (short) ioStatusResp.Az2motorCurrent;
  el1motorcurrent = (short) ioStatusResp.El1motorCurrent;
  el2motorcurrent = (short) ioStatusResp.El2motorCurrent;
  el3motorcurrent = (short) ioStatusResp.El3motorCurrent;
  el4motorcurrent = (short) ioStatusResp.El4motorCurrent;

  az1motorcurrentF = (float)az1motorcurrent / 10.0;
  az2motorcurrentF = (float)az2motorcurrent / 10.0;
  el1motorcurrentF = (float)el1motorcurrent / 10.0;
  el2motorcurrentF = (float)el2motorcurrent / 10.0;
  el3motorcurrentF = (float)el3motorcurrent / 10.0;
  el4motorcurrentF = (float)el4motorcurrent / 10.0;

  dsm_status = dsm_write(ACC,"DSM_AZ_MOTOR1_TEMP_S",&azmotor1temp);
  dsm_status = dsm_write(ACC,"DSM_AZ_MOTOR2_TEMP_S",&azmotor2temp);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR1_TEMP_S",&elmotor1temp);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR2_TEMP_S",&elmotor2temp);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR3_TEMP_S",&elmotor3temp);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR4_TEMP_S",&elmotor4temp);
        sprintf(redisData,"HSET acu azMotor1Temp %h",azmotor1temp);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu azMotor2Temp %h",azmotor2temp);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor1Temp %h",elmotor1temp);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor2Temp %h",elmotor2temp);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor3Temp %h",elmotor3temp);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor4Temp %h",elmotor4temp);
        redisResp = redisCommand(redisC,redisData);

  dsm_status = dsm_write(ACC,"DSM_AZ_MOTOR1_CURRENT_F",&az1motorcurrentF);
  dsm_status = dsm_write(ACC,"DSM_AZ_MOTOR2_CURRENT_F",&az2motorcurrentF);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR1_CURRENT_F",&el1motorcurrentF);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR2_CURRENT_F",&el2motorcurrentF);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR3_CURRENT_F",&el3motorcurrentF);
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR4_CURRENT_F",&el4motorcurrentF);
        sprintf(redisData,"HSET acu azMotor1Current %f",az1motorcurrentF);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu azMotor2Current %f",az2motorcurrentF);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor1Current %f",el1motorcurrentF);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor2Current %f",el2motorcurrentF);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor3Current %f",el3motorcurrentF);
        redisResp = redisCommand(redisC,redisData);
        sprintf(redisData,"HSET acu elMotor4Current %f",el4motorcurrentF);
        redisResp = redisCommand(redisC,redisData);

  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! dsm_status=%d\n",dsm_status);
  }


  } /* if recvBuff[0]  0x6 check */

  if (recvBuff[0]==0x2) {
  sprintf(acuErrorMessage,"ACU refuses the command from ACUstatus...reason:");
  dsm_status = dsm_write(ACC,"DSM_ACU_ERROR_MESSAGE_C256",acuErrorMessage);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_ERROR_MESSAGE_C256 dsm_status=%d\n",dsm_status);
  }
  sleep(2);
  if (recvBuff[1]==0x43) sprintf(acuErrorMessage,"Checksum error.\n");
  if (recvBuff[1]==0x45) sprintf(acuErrorMessage,"ETX not received at expected position.\n");
  if (recvBuff[1]==0x49) sprintf(acuErrorMessage,"Unknown identifier.\n");
  if (recvBuff[1]==0x4C) sprintf(acuErrorMessage,"Wrong length (incorrect no. of bytes rcvd.\n");
  if (recvBuff[1]==0x6C) sprintf(acuErrorMessage,"Specified length does not match identifier.\n");
  if (recvBuff[1]==0x4D) sprintf(acuErrorMessage,"Command ignored in present operating mode.\n");
  if (recvBuff[1]==0x6F) sprintf(acuErrorMessage,"Other reasons.\n");
  if (recvBuff[1]==0x52) sprintf(acuErrorMessage,"Device not in Remote mode.\n");
  if (recvBuff[1]==0x72) sprintf(acuErrorMessage,"Value out of range.\n");
  if (recvBuff[1]==0x53) sprintf(acuErrorMessage,"Missing STX.\n");
  } else {sprintf(acuErrorMessage,"");}

  dsm_status = dsm_write(ACC,"DSM_ACU_ERROR_MESSAGE_C256",acuErrorMessage);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_ERROR_MESSAGE_C256 dsm_status=%d\n",dsm_status);
  }

        usleep(100000);
	} /* while loop */

	pthread_detach(ACUiostatusTID);
	pthread_exit((void *) 0);
}

void *ACUmetrology() {
/* issue the V command to ACU for metrology data */
/* capture the response into metrologyDataSet struct */
/* extract various values and write them to DSM */

  int i,n = 0;
  char recvBuff[256];
  char sendBuff[256];
  int dsm_status,dsm_open_status;
  acuStatus acuStatusResp;
  acuCmd acuCommand={};
  metrologyData metrologyDataSet;
  short checksum;

  char acuErrorMessage[256],generalMetStatus[4];
  int tilt1x,tilt1y,tilt2x,tilt2y,tilt3x,tilt3y;
  int tilt1Temp,tilt2Temp,tilt3Temp;
  int linearSensor1,linearSensor2,linearSensor3,linearSensor4;
  int tiltAzCorr,tiltElCorr;
  int linearSensorAzCorr,linearSensorElCorr;
  int SPEMazCorr,SPEMelCorr;
  short tempSensor[41];
  
  acuCommand.stx = 0x2;
  acuCommand.id = 0x76;
  acuCommand.datalength = htole16(0x7);
/*
  acuCommand.checksum = 0x7D;
*/
 /*
  acuCommand.checksum = htole16(calculate_checksum(
                   (char *)&acuCommand.id, offsetof(acuCmd, checksum) - offsetof(acuCmd,id)));
 */
  acuCommand.etx = 0x3;

  checksum = checkSum((char*)(&acuCommand), sizeof(acuCommand));

   if(checksum > 0xffff) checksum=checksum & 0xffff;

   acuCommand.checksum = htole16(checksum);



  memset(recvBuff, '0' ,sizeof(recvBuff));
  memset(sendBuff, '0' ,sizeof(sendBuff));

  memcpy(sendBuff,(char*)&acuCommand,sizeof(acuCommand));

  while(1) {
	
  n = send(sockfdControl,sendBuff,sizeof(acuCommand),0);
  if (n<0) printf("ERROR writing to ACU.");
/*
  printf("Wrote %d bytes to ACU\n",n);
*/


  /* receive the ACK response from ACU */
  n = recv(sockfdControl, recvBuff, sizeof(metrologyDataSet),0);

  if( n < 0)  printf("\n Read Error \n");

  /* check if ACK is received, then receive the response and parse it */
  if (recvBuff[0]==0x6) {

  n = recv(sockfdControl, (char *)&metrologyDataSet,sizeof(metrologyDataSet),0);
  
  tilt1x=metrologyDataSet.tilt1x;
  tilt1y=metrologyDataSet.tilt1y;
  tilt2x=metrologyDataSet.tilt2x;
  tilt2y=metrologyDataSet.tilt2y;
  tilt3x=metrologyDataSet.tilt3x;
  tilt3y=metrologyDataSet.tilt3y;

  tilt1Temp=metrologyDataSet.tilt1Temp;
  tilt2Temp=metrologyDataSet.tilt2Temp;
  tilt3Temp=metrologyDataSet.tilt3Temp;

  linearSensor1=metrologyDataSet.linearSensor1;
  linearSensor2=metrologyDataSet.linearSensor2;
  linearSensor3=metrologyDataSet.linearSensor3;
  linearSensor4=metrologyDataSet.linearSensor4;

  tiltAzCorr=metrologyDataSet.tiltAzCorr;
  tiltElCorr=metrologyDataSet.tiltElCorr;

  linearSensorAzCorr=metrologyDataSet.linearSensorAzCorr;
  linearSensorElCorr=metrologyDataSet.linearSensorElCorr;

  SPEMazCorr=metrologyDataSet.SPEMazCorr;
  SPEMelCorr=metrologyDataSet.SPEMelCorr;

  for(i=0;i<41;i++) {
  tempSensor[i]=metrologyDataSet.tempSensor[i];
  }

/*debug 
printf("tilt1x=%d,tilt1y=%d,tilt2x=%d,tilt2y=%d,tilt3x=%d,tilt3y=%d\n",tilt1x,tilt1y,tilt2x,tilt2y,tilt3x,tilt3y);
printf("tilt1temp=%d,tilt2temp=%d,tilt3temp=%d\n",tilt1Temp,tilt2Temp,tilt3Temp);
printf("linearSensor1=%d,linearSensor2=%d,linearSensor3=%d,linearSensor4=%d\n",linearSensor1,linearSensor2,linearSensor3,linearSensor4);
printf("tempSensor1=%d,tempSensor2=%d,tempSensor3=%d\n",tempSensor[0],tempSensor[1],tempSensor[2]);
*/

  dsm_status = dsm_write(ACC,"DSM_MET_TEMP_SENSOR_V41_S",tempSensor);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_MET_TEMP_SENSOR_V41_S dsm_status=%d\n",dsm_status);
  }

  dsm_status = dsm_write(ACC,"DSM_SPEM_AZCORR_L",&SPEMazCorr);
  dsm_status = dsm_write(ACC,"DSM_SPEM_ELCORR_L",&SPEMelCorr);
  dsm_status = dsm_write(ACC,"DSM_LINEAR_SENSOR_AZCORR_L",&linearSensorAzCorr);
  dsm_status = dsm_write(ACC,"DSM_LINEAR_SENSOR_ELCORR_L",&linearSensorElCorr);
  dsm_status = dsm_write(ACC,"DSM_TILTAZCORR_L",&tiltAzCorr);
  dsm_status = dsm_write(ACC,"DSM_TILTELCORR_L",&tiltElCorr);
  dsm_status = dsm_write(ACC,"DSM_LINEAR_SENSOR1_L",&linearSensor1);
  dsm_status = dsm_write(ACC,"DSM_LINEAR_SENSOR2_L",&linearSensor2);
  dsm_status = dsm_write(ACC,"DSM_LINEAR_SENSOR3_L",&linearSensor3);
  dsm_status = dsm_write(ACC,"DSM_LINEAR_SENSOR4_L",&linearSensor4);
  dsm_status = dsm_write(ACC,"DSM_TILT1TEMP_L",&tilt1Temp);
  dsm_status = dsm_write(ACC,"DSM_TILT2TEMP_L",&tilt2Temp);
  dsm_status = dsm_write(ACC,"DSM_TILT3TEMP_L",&tilt3Temp);
  dsm_status = dsm_write(ACC,"DSM_TILT1X_L",&tilt1x);
  dsm_status = dsm_write(ACC,"DSM_TILT1Y_L",&tilt1y);
  dsm_status = dsm_write(ACC,"DSM_TILT2X_L",&tilt2x);
  dsm_status = dsm_write(ACC,"DSM_TILT2Y_L",&tilt2y);
  dsm_status = dsm_write(ACC,"DSM_TILT3X_L",&tilt3x);
  dsm_status = dsm_write(ACC,"DSM_TILT3Y_L",&tilt3y);


  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! metrology variables dsm_status=%d\n",dsm_status);
  }

        
        redisWriteInt("acu","spemAzCorr",SPEMazCorr);
        redisWriteInt("acu","spemElCorr",SPEMelCorr);
        redisWriteInt("acu","linearSensorAzCorr",linearSensorAzCorr);
        redisWriteInt("acu","linearSensorElCorr",linearSensorElCorr);
        redisWriteInt("acu","tiltAzCorr",tiltAzCorr);
        redisWriteInt("acu","tiltElCorr",tiltElCorr);
        redisWriteInt("acu","linearSensor1",linearSensor1);
        redisWriteInt("acu","linearSensor2",linearSensor2);
        redisWriteInt("acu","linearSensor3",linearSensor3);
        redisWriteInt("acu","linearSensor4",linearSensor4);
        redisWriteInt("acu","tilt1Temp",tilt1Temp);
        redisWriteInt("acu","tilt2Temp",tilt2Temp);
        redisWriteInt("acu","tilt3Temp",tilt3Temp);
        redisWriteInt("acu","tilt1x",tilt1x);
        redisWriteInt("acu","tilt1y",tilt1y);
        redisWriteInt("acu","tilt2x",tilt2x);
        redisWriteInt("acu","tilt2y",tilt2y);
        redisWriteInt("acu","tilt3x",tilt3x);
        redisWriteInt("acu","tilt3y",tilt3y);

  }

  if (recvBuff[0]==0x2) {
  sprintf(acuErrorMessage,"ACU refuses the command...reason:");
  dsm_status = dsm_write(ACC,"DSM_ACU_ERROR_MESSAGE_C256",acuErrorMessage);
        sprintf(redisData,"HSET acu errorMessage %s",acuErrorMessage);
        redisResp = redisCommand(redisC,redisData);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_ERROR_MESSAGE_C256 dsm_status=%d\n",dsm_status);
  }
  if (recvBuff[1]==0x43) sprintf(acuErrorMessage,"Checksum error.\n");
  if (recvBuff[1]==0x45) sprintf(acuErrorMessage,"ETX not received at expected position.\n");
  if (recvBuff[1]==0x49) sprintf(acuErrorMessage,"Unknown identifier.\n");
  if (recvBuff[1]==0x4C) sprintf(acuErrorMessage,"Wrong length (incorrect no. of bytes rcvd.\n");
  if (recvBuff[1]==0x6C) sprintf(acuErrorMessage,"Specified length does not match identifier.\n");
  if (recvBuff[1]==0x4D) sprintf(acuErrorMessage,"Command ignored in present operating mode.\n");
  if (recvBuff[1]==0x6F) sprintf(acuErrorMessage,"Other reasons.\n");
  if (recvBuff[1]==0x52) sprintf(acuErrorMessage,"Device not in Remote mode.\n");
  if (recvBuff[1]==0x72) sprintf(acuErrorMessage,"Value out of range.\n");
  if (recvBuff[1]==0x53) sprintf(acuErrorMessage,"Missing STX.\n");
  } else {sprintf(acuErrorMessage,"");}

  dsm_status = dsm_write(ACC,"DSM_ACU_ERROR_MESSAGE_C256",acuErrorMessage);
        sprintf(redisData,"HSET acu errorMessage %s",acuErrorMessage);
        redisResp = redisCommand(redisC,redisData);
  if (dsm_status != DSM_SUCCESS) {
  printf("DSM write failed! DSM_ACU_ERROR_MESSAGE_C256 dsm_status=%d\n",dsm_status);
  }
/*
        usleep(2000000);
*/
        sleep(1);
	} /* while loop */

	pthread_detach(ACUmetrologyTID);
	pthread_exit((void *) 0);
}

void *ACUConnectionWatchdog(void *arg) {
    struct sockaddr_in serv_addr_control, serv_addr_monitor;
    int retry_interval_sec = 5;

    serv_addr_control.sin_family = AF_INET;
    serv_addr_control.sin_port = htons(ACU_CONTROL_PORT);
    serv_addr_control.sin_addr.s_addr = inet_addr(ACU_IP_ADDRESS);

    serv_addr_monitor.sin_family = AF_INET;
    serv_addr_monitor.sin_port = htons(ACU_MONITOR_PORT);
    serv_addr_monitor.sin_addr.s_addr = inet_addr(ACU_IP_ADDRESS);

    while (1) {
        struct pollfd pfd;
        pfd.fd = sockfdControl;
        pfd.events = POLLOUT;

        int poll_status = poll(&pfd, 1, 1000);  // 1s timeout
        if (poll_status <= 0 || (pfd.revents & (POLLERR | POLLHUP))) {
            fprintf(stderr, "[ACU Watchdog] Connection lost. Attempting reconnection...\n");
            close(sockfdControl);
            close(sockfdMonitor);

            // Retry connecting to CONTROL port
            while (1) {
                sockfdControl = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfdControl >= 0 &&
                    connect(sockfdControl, (struct sockaddr *)&serv_addr_control, sizeof(serv_addr_control)) == 0) {
                    fprintf(stderr, "[ACU Watchdog] Reconnected to ACU CONTROL port.\n");
                    break;
                }
                close(sockfdControl);
                sleep(retry_interval_sec);
            }

            // Retry connecting to MONITOR port
            while (1) {
                sockfdMonitor = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfdMonitor >= 0 &&
                    connect(sockfdMonitor, (struct sockaddr *)&serv_addr_monitor, sizeof(serv_addr_monitor)) == 0) {
                    fprintf(stderr, "[ACU Watchdog] Reconnected to ACU MONITOR port.\n");
                    break;
                }
                close(sockfdMonitor);
                sleep(retry_interval_sec);
            }
        }

        sleep(2);  // Polling interval
    }

    return NULL;
}


int ACUmode(int commandCode) {

  int n = 0;
  char recvBuff[256];
  char sendBuff[256];
  struct sockaddr_in serv_addr;
  acuStatus acuStatusResp;
  acuCmd acuCommand={};
  acuModeCmd acuModeCommand={};
  short checksum;

  printf(".........................got ACU mode command: %d\n",commandCode);

/*
  printf("Enter the command code:\n");
  printf("(1) Stop\n");
  printf("(2) Maintenance\n");
  printf("(3) Preset\n");
  printf("(4) Program Track\n");
  printf("(5) Rate \n");
  printf("(6) Sector scan\n");
  printf("(7) Survival stow\n");
  printf("(8) Maintenance stow\n");
  printf("(9) Unstow \n");
  scanf("%d",&commandCode);
*/


  acuModeCommand.stx = 0x2;
  acuModeCommand.id = 0x4d; /* page 13 of ICD section 4.1.1.1  M cmd */
  acuModeCommand.datalength = htole16(0xc);
  acuModeCommand.polMode = 0x0;
  acuModeCommand.controlWord = 0x0;
  acuModeCommand.etx = 0x3;
  acuModeCommand.azMode=0x0;
  acuModeCommand.elMode=0x0;

   /* for now, setting both az and el modes to be same */
  switch (commandCode) {
  
  case 1:
      acuModeCommand.azMode = 0x1;   /* stop */
      break;
  case 2:
      acuModeCommand.azMode = 0x21;  /* maintenance */
      break;
  case 3:
      acuModeCommand.azMode = 0x2;   /* preset */
      break;
  case 4:
      acuModeCommand.azMode = 0x3;  /* program track */
      break;
  case 5:
      acuModeCommand.azMode = 0x4;  /* rate */
      break;
  case 6: 
      acuModeCommand.azMode = 0x5;  /* sector scan */
      break;
  case 7:
      acuModeCommand.azMode = 0x6;  /* survival stow */
      break;
  case 8:
      acuModeCommand.azMode = 0xe;  /* maint. stow */
      break;
  case 9:
      acuModeCommand.azMode = 0x26; /* unstow */
      break;
  case 10:
      printf("got shutter open command \n");
      acuModeCommand.azMode = 0x0; /* shutter open/close */
      acuModeCommand.controlWord = 0x4000; /* shutter open */
      break;
  case 11:
      printf("got shutter close command \n");
      acuModeCommand.azMode = 0x0; /* shutter open/close */
      acuModeCommand.controlWord = 0x8000; /* shutter close */
      break;
  case  12:
      acuModeCommand.azMode = 0x22; /* ACU servo self test */
      break;
  }

  printf("sending preset code: 0x%x\n",acuModeCommand.azMode);
  acuModeCommand.elMode=acuModeCommand.azMode;
/*
  acuModeCommand.checksum = acuModeCommand.id + acuModeCommand.datalength+
                         acuModeCommand.azMode+acuModeCommand.elMode;
*/

   checksum = checkSum((char*)(&acuModeCommand), sizeof(acuModeCommand));

   if(checksum > 0xffff) checksum=checksum & 0xffff;

   acuModeCommand.checksum = htole16(checksum);

   /*
   acuModeCommand.checksum = htole16(calculate_checksum(
                   (char *)&acuModeCommand.id, offsetof(acuCmd, checksum) - offsetof(acuCmd,id)));
   */


  memset(recvBuff, '0' ,sizeof(recvBuff));
  memset(sendBuff, '0' ,sizeof(sendBuff));

  memcpy(sendBuff,(char*)&acuModeCommand,sizeof(acuModeCommand));
  n = send(sockfdControl,sendBuff,sizeof(acuModeCommand),0);
  if (n<0) printf("ERROR writing to ACU.");
 
  /* receive the ACK response from ACU */
  n = recv(sockfdControl, recvBuff, sizeof(acuStatusResp),0); 
/*
  printf("Received:  0x%x from ACU\n",recvBuff[0]);
*/

  if( n < 0)  printf("\n Read Error \n"); 

  /* check if ACK is received, then receive the response and parse it */
  if (recvBuff[0]==0x6) {

  printf("ACU: ACK, OK \n");
  } else {
  printf("ACU refuses the command from ACUmode...reason:");
  printf("Received:  0x%x 0x%x from ACU\n",recvBuff[0],recvBuff[1]);
  if (recvBuff[1]==0x43) printf("Checksum error.\n");
  if (recvBuff[1]==0x45) printf("ETX not received at expected position.\n");
  if (recvBuff[1]==0x49) printf("Unknown identifier.\n");
  if (recvBuff[1]==0x4C) printf("Wrong length (incorrect no. of bytes rcvd.\n");
  if (recvBuff[1]==0x6C) printf("Specified length does not match identifier.\n");
  if (recvBuff[1]==0x4D) printf("Command ignored in present operating mode.\n");
  if (recvBuff[1]==0x6F) printf("Other reasons.\n");
  if (recvBuff[1]==0x52) printf("Device not in Remote mode.\n");
  if (recvBuff[1]==0x72) printf("Value out of range.\n");
  if (recvBuff[1]==0x53) printf("Missing STX.\n");
  }

 
  return 0;
}

/* ACU Program track mode */
int ACUprogTrack(int numPos, int timeOfDay[], int azProgTrack[], int elProgTrack[],short clearstack) {
  int n = 0;
  char recvBuff[256];
  char sendBuff[256];
  struct sockaddr_in serv_addr;
  acuStatus acuStatusResp;
  acuCmd acuCommand = {};
  acuAzElProg acuAzElProgCommand ={};
  acuAzElProgTraj acuAzElProgCommandTraj = {};
  short checksum=0;
  int acuTimeOfDay=0;
  int trackTime,newtime;
  int i,az,el;
  int dsm_status;
  int acuDay;
  time_t timestamp;

  memset(recvBuff, '0' ,sizeof(recvBuff));
  memset(sendBuff, '0' ,sizeof(sendBuff));

   dsm_status=dsm_read(DSM_HOST,"DSM_ACU_DAYOFYEAR_L",&acuDay,&timestamp);
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_read() DSM_ACU_DAYOFYEAR_L");
                exit(1);
        }

  /* now pass each az/el value to acu with newtime stamps */
 if (numPos==1) {
  acuAzElProgCommand.stx = 0x2;
  acuAzElProgCommand.id = 0x4F; /* page 16 of ICD section 4.1.1.4  O cmd */
  acuAzElProgCommand.datalength = htole16(0x17);
  acuAzElProgCommand.clearstack=0x1;
  acuAzElProgCommand.timeOfDay=timeOfDay[0];
  acuAzElProgCommand.dayOfYear=(short)acuDay;
  acuAzElProgCommand.cmdAz = azProgTrack[0];
  acuAzElProgCommand.cmdEl = elProgTrack[0];
  acuAzElProgCommand.etx = 0x3;

  checksum = checkSum((char*)(&acuAzElProgCommand), sizeof(acuAzElProgCommand));
  if(checksum > 0xffff) checksum=checksum & 0xffff;
  acuAzElProgCommand.checksum = htole16(checksum);

 /*
  acuAzElProgCommand.checksum = htole16(calculate_checksum(
                (char *)&acuAzElProgCommand.id, offsetof(acuCmd, checksum) - offsetof(acuCmd,id)));
*/

   memcpy(sendBuff,(char*)&acuAzElProgCommand,sizeof(acuAzElProgCommand));
     n = send(sockfdControl,sendBuff,sizeof(acuAzElProgCommand),0);
     if (n<0) printf("ERROR writing to ACU. For acuAzElProgCommand....");
/*
     printf("Wrote %d bytes to ACU - acuAzElProgCommand....\n",n);
*/
 } else {

  acuAzElProgCommandTraj.stx = 0x2;
  acuAzElProgCommandTraj.id = 0x42; /* page 17 of ICD section 4.1.1.5  B cmd */
  acuAzElProgCommandTraj.datalength = 0x5d; /* (n*14+9)= 93  in hex; n=6 */
  acuAzElProgCommandTraj.numPos = (short)numPos;

  acuAzElProgCommandTraj.timeOfDay0=timeOfDay[0];
  acuAzElProgCommandTraj.dayOfYear0=(short)acuDay;
  acuAzElProgCommandTraj.cmdAz0 = azProgTrack[0];
  acuAzElProgCommandTraj.cmdEl0 = elProgTrack[0];

  acuAzElProgCommandTraj.timeOfDay1=timeOfDay[1];
  acuAzElProgCommandTraj.dayOfYear1=(short)acuDay;
  acuAzElProgCommandTraj.cmdAz1 = azProgTrack[1];
  acuAzElProgCommandTraj.cmdEl1 = elProgTrack[1];

  acuAzElProgCommandTraj.timeOfDay2=timeOfDay[2];
  acuAzElProgCommandTraj.dayOfYear2=(short)acuDay;
  acuAzElProgCommandTraj.cmdAz2 = azProgTrack[2];
  acuAzElProgCommandTraj.cmdEl2 = elProgTrack[2];

  acuAzElProgCommandTraj.timeOfDay3=timeOfDay[3];
  acuAzElProgCommandTraj.dayOfYear3=(short)acuDay;
  acuAzElProgCommandTraj.cmdAz3 = azProgTrack[3];
  acuAzElProgCommandTraj.cmdEl3 = elProgTrack[3];

  acuAzElProgCommandTraj.timeOfDay4=timeOfDay[4];
  acuAzElProgCommandTraj.dayOfYear4=(short)acuDay;
  acuAzElProgCommandTraj.cmdAz4 = azProgTrack[4];
  acuAzElProgCommandTraj.cmdEl4 = elProgTrack[4];

  acuAzElProgCommandTraj.timeOfDay5=timeOfDay[5];
  acuAzElProgCommandTraj.dayOfYear5=(short)acuDay;
  acuAzElProgCommandTraj.cmdAz5 = azProgTrack[5];
  acuAzElProgCommandTraj.cmdEl5 = elProgTrack[5];
  acuAzElProgCommandTraj.etx = 0x3;

  checksum = checkSum((char*)(&acuAzElProgCommandTraj), sizeof(acuAzElProgCommandTraj));
  if(checksum > 0xffff) checksum=checksum & 0xffff;
  acuAzElProgCommandTraj.checksum = htole16(checksum);

   memcpy(sendBuff,(char*)&acuAzElProgCommandTraj,sizeof(acuAzElProgCommandTraj));
     n = send(sockfdControl,sendBuff,sizeof(acuAzElProgCommandTraj),0);
     if (n<0) printf("ERROR writing to ACU. acuAzElprogCommandTraj.......");
/*
     printf("Wrote %d bytes to ACU acuAzElprogCommandTraj.......\n",n);
*/

 }

     /* receive the ACK response from ACU */
     n = recv(sockfdControl, recvBuff, sizeof(acuStatusResp),0);

     if( n < 0)  printf("\n Read Error \n");

/*check if ACK is received, then receive the response and parse it */
     if (recvBuff[0]==0x6) {

/*
     printf("ACU: ACK, OK \n");
*/
     }
      else {
      printf("ACU refuses the Program Track command from ACUprogTrack...reason:");
      printf("Received:  0x%x 0x%x from ACU\n",recvBuff[0],recvBuff[1]);
      if (recvBuff[1]==0x43) printf("Checksum error.\n");
      if (recvBuff[1]==0x45) printf("ETX not received at expected position.\n");
      if (recvBuff[1]==0x49) printf("Unknown identifier.\n");
      if (recvBuff[1]==0x4C) printf("Wrong length (incorrect no. of bytes rcvd.\n");
      if (recvBuff[1]==0x6C) printf("Specified length does not match identifier.\n");
      if (recvBuff[1]==0x4D) printf("Command ignored in present operating mode.\n");
      if (recvBuff[1]==0x6F) printf("Other reasons.\n");
      if (recvBuff[1]==0x52) printf("Device not in Remote mode.\n");
      if (recvBuff[1]==0x72) printf("Value out of range.\n");
      if (recvBuff[1]==0x53) printf("Missing STX.\n");
      }
return(0);
}

/* Az El (preset mode)  command */
int ACUAzEl(double cmdAzdeg, double cmdEldeg) {

  int n = 0;
  char recvBuff[256];
  char sendBuff[256];
  struct sockaddr_in serv_addr;
  acuStatus acuStatusResp;
  acuAzElCmd acuAzElCommand ={0,0,0,0,0,0,0,0};
  short checksum;

  if((cmdAzdeg<AZ_CCW_LIMIT)||(cmdAzdeg>AZ_CW_LIMIT)) {
  printf("Invalid commanded az; should be between -180 and 360 deg.\n");
  exit(0);
  }

  if((cmdEldeg<EL_DOWN_LIMIT)||(cmdEldeg>EL_UP_LIMIT)) {
  printf("Invalid commanded el; beyond limits.\n");
  exit(0);
  }

  cmdAzdeg *= 1.0e6;
  cmdEldeg *= 1.0e6;

  acuAzElCommand.cmdAz = (int)cmdAzdeg;
  acuAzElCommand.cmdEl = (int)cmdEldeg;


  acuAzElCommand.stx = 0x2;
  acuAzElCommand.id = 0x50; /* page 14 of ICD section 4.1.1.2  P cmd */
  acuAzElCommand.datalength = htole16(0x13);
  acuAzElCommand.cmdPol = 0x0;
  acuAzElCommand.etx = 0x3;

  checksum = checkSum((char*)(&acuAzElCommand), sizeof(acuAzElCommand));

  if(checksum > 0xffff) checksum=checksum & 0xffff;
  
  acuAzElCommand.checksum = htole16(checksum);

 /*
  acuAzElCommand.checksum = htole16(calculate_checksum(
                   (char *)&acuAzElCommand.id, offsetof(acuCmd, checksum) - offsetof(acuCmd,id)));
 */

  memset(recvBuff, '0' ,sizeof(recvBuff));
  memset(sendBuff, '0' ,sizeof(sendBuff));

  memcpy(sendBuff,(char*)&acuAzElCommand,sizeof(acuAzElCommand));
  n = send(sockfdControl,sendBuff,sizeof(acuAzElCommand),0);
  if (n<0) printf("ERROR writing to ACU.");
 
  /* receive the ACK response from ACU */
  n = recv(sockfdControl, recvBuff, sizeof(acuStatusResp),0); 

  if( n < 0)  printf("\n Read Error \n"); 

  /* check if ACK is received, then receive the response and parse it */
  if (recvBuff[0]==0x6) {

/*
  printf("ACU: ACK, OK \n");
*/
  }

  if (recvBuff[0]!=0x6) {
  printf("ACU refuses the command from ACUAzEl...reason:");
  printf("Received:  0x%x 0x%x from ACU\n",recvBuff[0],recvBuff[1]);
  if (recvBuff[1]==0x43) printf("Checksum error.\n");
  if (recvBuff[1]==0x45) printf("ETX not received at expected position.\n");
  if (recvBuff[1]==0x49) printf("Unknown identifier.\n");
  if (recvBuff[1]==0x4C) printf("Wrong length (incorrect no. of bytes rcvd.\n");
  if (recvBuff[1]==0x6C) printf("Specified length does not match identifier.\n");
  if (recvBuff[1]==0x4D) printf("Command ignored in present operating mode.\n");
  if (recvBuff[1]==0x6F) printf("Other reasons.\n");
  if (recvBuff[1]==0x52) printf("Device not in Remote mode.\n");
  if (recvBuff[1]==0x72) printf("Value out of range.\n");
  if (recvBuff[1]==0x53) printf("Missing STX.\n");
  }

 
  return 0;
}

/* Az El rate (rate mode)  command */
int ACUAzElRate(double cmdAzRate, double cmdElRate) {

  int n = 0;
  char recvBuff[256];
  char sendBuff[256];
  struct sockaddr_in serv_addr;
  acuStatus acuStatusResp;
  acuAzElCmd acuAzElRateCommand ={0,0,0,0,0,0,0,0};
  short checksum;

 if((cmdAzRate<-6.)||(cmdAzRate>6.)) {
  printf("Invalid commanded az rate;  should be between -6 and 6 deg/s.\n");
  exit(0);
  }

  if((cmdElRate<-3.)||(cmdElRate>3.)) {
  printf("Invalid commanded el rate; should be between -3 and 3 deg/s.\n");
  exit(0);
  }

  cmdAzRate *= 1.0e6;
  cmdElRate *= 1.0e6;

  acuAzElRateCommand.cmdAz = (int)cmdAzRate;
  acuAzElRateCommand.cmdEl = (int)cmdElRate;


  acuAzElRateCommand.stx = 0x2;
  acuAzElRateCommand.id = 0x52; /* page 15 of ICD section 4.1.1.3  R cmd */
  acuAzElRateCommand.datalength = htole16(0x13);
  acuAzElRateCommand.cmdPol = 0x0;
  acuAzElRateCommand.etx = 0x3;

  checksum = checkSum((char*)(&acuAzElRateCommand), sizeof(acuAzElRateCommand));

  if(checksum > 0xffff) checksum=checksum & 0xffff;
  
  acuAzElRateCommand.checksum = htole16(checksum);
  /*
  acuAzElRateCommand.checksum = htole16(calculate_checksum(
              (char *)&acuAzElRateCommand.id, offsetof(acuCmd, checksum) - offsetof(acuCmd,id)));
  */

  memset(recvBuff, '0' ,sizeof(recvBuff));
  memset(sendBuff, '0' ,sizeof(sendBuff));

  memcpy(sendBuff,(char*)&acuAzElRateCommand,sizeof(acuAzElRateCommand));
  n = send(sockfdControl,sendBuff,sizeof(acuAzElRateCommand),0);
  if (n<0) printf("ERROR writing to ACU.");
 
  /* receive the ACK response from ACU */
  n = recv(sockfdControl, recvBuff, sizeof(acuStatusResp),0); 

  if( n < 0)  printf("\n Read Error \n"); 

  /* check if ACK is received, then receive the response and parse it */
  if (recvBuff[0]==0x6) {

  printf("ACU: ACK, OK from ACUAzElRate.\n");
  }

  if (recvBuff[0]!=0x6) {
  printf("ACU refuses the command from ACUAzElRate...reason:");
  printf("Received:  0x%x 0x%x from ACU\n",recvBuff[0],recvBuff[1]);
  if (recvBuff[1]==0x43) printf("Checksum error.\n");
  if (recvBuff[1]==0x45) printf("ETX not received at expected position.\n");
  if (recvBuff[1]==0x49) printf("Unknown identifier.\n");
  if (recvBuff[1]==0x4C) printf("Wrong length (incorrect no. of bytes rcvd.\n");
  if (recvBuff[1]==0x6C) printf("Specified length does not match identifier.\n");
  if (recvBuff[1]==0x4D) printf("Command ignored in present operating mode.\n");
  if (recvBuff[1]==0x6F) printf("Other reasons.\n");
  if (recvBuff[1]==0x52) printf("Device not in Remote mode.\n");
  if (recvBuff[1]==0x72) printf("Value out of range.\n");
  if (recvBuff[1]==0x53) printf("Missing STX.\n");
  }

 
  return 0;
}

short checkSum (char *buff,int size) {
  
  short i=0,sum=0;

      while(size) {
      sum = sum + (short) buff[i]; 
      size--;
      i++;
      }
  sum -= 5; /*subtract the sum of STX 0x2 and ETX 0x3, 
           first and last bytes */
  return sum;
}

/*The following checksum function is to replace the above one */
short calculate_checksum(char *buff, int size) {
    short sum = 0;
    for (int i = 0; i < size; i++) {
        sum += (signed char)buff[i]; // Treat as 8-bit signed integers
    }
    return (short)(sum & 0xFFFF); // Lower 16 bits only
}

void handlerForSIGINT(int signum)
{
        interrupt_command_flag=1;
        user='q'; /* 'q' for quit command */
	receivedSignal=signum;
        fprintf(stderr,"Got the control C signal:%d. Quitting.\n",signum);
}

double tjd2et(double tjd)
{
  return((tjd-2451545.)* 86400.0+32.184+LEAPSECONDS);
}

/*********************************end of track.c*************************/
