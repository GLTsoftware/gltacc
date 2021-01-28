#ifndef _vme_sg
#define _vme_sg

	
/* VME-SG Registers */
typedef struct {
	volatile unsigned short ctl[4];
	volatile unsigned short vec[4];
	volatile unsigned short dummy1[24];
	volatile unsigned short time_1;
	volatile unsigned short freeze_1_1;
	volatile unsigned short release_1;
	volatile unsigned short freeze_2_1;
	volatile unsigned short release_2;
	volatile unsigned short dummy2[27];
	volatile unsigned short freeze_1_2;
	volatile unsigned short freeze_1_3;
	volatile unsigned short freeze_1_4;
	volatile unsigned short freeze_1_5;
	volatile unsigned short freeze_2_2;
	volatile unsigned short freeze_2_3;
	volatile unsigned short freeze_2_4;
	volatile unsigned short freeze_2_5;
	volatile unsigned short lat_1;
	volatile unsigned short lat_2;
	volatile unsigned short lat_3;
	volatile unsigned short lon_1;
	volatile unsigned short lon_2;
	volatile unsigned short lon_3;
	volatile unsigned short ele_1;
	volatile unsigned short ele_2;
	volatile unsigned short cfg_1;
	volatile unsigned short cfg_2;
	volatile unsigned short p_mode;
	volatile unsigned short tim_off;
	volatile unsigned short pls_rate;
	volatile unsigned short dac;
	volatile unsigned short preset[9];
	volatile unsigned short u_flag;
	volatile unsigned short dummy3[8];
	volatile unsigned short ph_comp;
	volatile unsigned short tim_bias[2];
	volatile unsigned short num_sat;
	volatile unsigned short rsv[3];
	volatile unsigned short self_test;
	volatile unsigned short coin_cmp_1[8];
	volatile unsigned short coin_cmp_2[8];
	} VME_SG;

unsigned short dummy_read;

/* 2.4.10 VME INTERRUPT CONTROL */
#define BIM_CTL_1 sg_ptr->ctl[0]
#define BIM_CTL_2 sg_ptr->ctl[1]
#define BIM_CTL_3 sg_ptr->ctl[2]
#define BIM_CTL_4 sg_ptr->ctl[3]
#define BIM_VEC_1 sg_ptr->vec[0]
#define BIM_VEC_2 sg_ptr->vec[1]
#define BIM_VEC_3 sg_ptr->vec[2]
#define BIM_VEC_4 sg_ptr->vec[3]
#define IRQ_DISABLED 0
#define IRQ_1 1
#define IRQ_2 2
#define IRQ_3 3
#define IRQ_4 4
#define IRQ_5 5
#define IRQ_6 6
#define IRQ_7 7
#define AUTO_CLEAR 0x8
#define INT_ENA 0x10
#define NO_VECTOR 0x20
#define FLAG_AUTO_CLEAR 0x40

/* 2.4.6 READING TIME */
#define TIME_REQ dummy_read = (sg_ptr->time_1)
#define TIME_REL dummy_read = (sg_ptr->release_1)
#define USEC sg_ptr->freeze_1_1
#define TEN_MSEC sg_ptr->freeze_1_2
#define HRS_MINS sg_ptr->freeze_1_3
#define STAT_DAYS sg_ptr->freeze_1_4
#define YEARS sg_ptr->freeze_1_5

#define TIME_REL_EXT sg_ptr->release_2
#define USEC_EXT sg_ptr->freeze_2_1
#define TEN_MSEC_EXT sg_ptr->freeze_2_2
#define HRS_MINS_EXT sg_ptr->freeze_2_3
#define STAT_DAYS_EXT sg_ptr->freeze_2_4
#define YEARS_EXT sg_ptr->freeze_2_5

#define REF_ERR 0x1000
#define PH_LOCK 0x2000



/* 2.4.9 OPERATIONAL CONFIGURATION AND STATUS */
#define CFG_1 sg_ptr->cfg_1
#define GEN_MODE 0x0
#define GPS_MODE 0x1
#define IRIG_MODE 0x2
#define GEN_STOP 0x4
#define GEN_CLEAR 0x8
#define EXT_START 0x10
#define EXT_OSC_SEL 0x20
#define INT_ENABLE 0x80

#define FALL_EDGE 0x4
#define DC_SHIFT 0x8
#define POS_INHIBIT 0x10
#define POS_LOAD 0x20
#define CLEAR_INTERVAL 0x40
#define GPS_TIME 0x80

/* Position Mode */
#define KNOWN 0
#define SURVEY 1
#define AUTOMATIC 2
#define DYNAMIC 3

/* Rate Output */
#define PLS_RATE sg_ptr->pls_rate
#define RATE_DISABLED 0
#define RATE_100KHZ 1
#define RATE_1000HZ 2
#define RATE_100HZ 3
#define RATE_10HZ 4
#define RATE_1HZ 5

/* Self Test */
#define NO_ERROR 0
#define RAM_ERROR 1
#define CLK_ERROR 2
#define COM_ERROR 3
#define OPR_ERROR 4
#define DAC_ERROR 5

/* IOCTL */
#define GET_PT 0
#define GET_TIME 1
#define SET_DAC 2
#define SERVO_CMD 3
#define SET_TURN 4
#define GET_CONF 5
#define SET_YEAR 6
#define GET_SATS 7
#define SET_GENERATOR 8
#define SET_GPS_SYNCHRONIZED 9
#define SET_IRIG_SYNCHRONIZED 10

/* Position-Time */
typedef struct {
	long az;    		/* pos 1 */
	long el;    		/* pos 2 */
	unsigned long useconds;
	unsigned short minutes;
	unsigned short hours;
	unsigned short days;
	unsigned short years; /* Introducing the Y32.768K Problem */
	int irig_stat;			/* Time Status */
	long az_sim;
	long el_sim;
	} POS_TIME;

#define IRIG_ERROR 0
#define IRIG_OK 1

/* Set DAC */
typedef short DAC_ARRAY[3], DAC_TYPE;

/* Servo Command */
typedef struct {
	long pos_cmd[2];
	short dac_3;
	short bias[2];
	} POS_CMD;

/* Set Turns */
typedef short TURN_TYPE[2], TURN;

/* Set Year */
typedef unsigned short YEAR_TYPE;


/* SELECT */

typedef struct {
	int dummy_1;
	int dummy_2;
	} sel;

/* Encoder */
#define FULL_TURN 4194304
#define HALF_TURN 2097152

#endif
