/* In the following, id, datalength and checksum are set
for each command as needed */

typedef struct __attribute__ ((packed)) acuCmd {
    char stx;
    char id;
    short datalength;
    short checksum;
    char etx;
} acuCmd; 


typedef struct __attribute__ ((packed)) acuModeCmd {
    char stx;
    char id;
    short datalength;
    char azMode;
    char elMode;
    char polMode;
    short controlWord;
    short checksum;
    char etx;
} acuModeCmd; 

typedef struct __attribute__ ((packed)) acuAuxCmd {
    char stx;
    char id;
    short datalength;
    char azMode;
    char elMode;
    short checksum;
    char etx;
} acuAuxCmd; 

typedef struct __attribute__ ((packed)) acuAzElCmd {
    char stx;
    char id;
    short datalength;
    int cmdAz __attribute__ ((packed));
    int cmdEl __attribute__ ((packed));
    int cmdPol __attribute__ ((packed));
    short checksum;
    char etx;
} acuAzElCmd; 

typedef struct __attribute__ ((packed)) acuAzElProg {
    char stx;
    char id;
    short datalength;
    short clearstack;
    int timeOfDay __attribute__ ((packed));
    short dayOfYear;
    int cmdAz __attribute__ ((packed));
    int cmdEl __attribute__ ((packed));
    short checksum;
    char etx;
} acuAzElProg; 

typedef struct __attribute__ ((packed)) acuAzElProgTraj {
    char stx;
    char id;
    short datalength;
    short numPos;

    int timeOfDay0 __attribute__ ((packed));
    short dayOfYear0;
    int cmdAz0 __attribute__ ((packed));
    int cmdEl0 __attribute__ ((packed));

    int timeOfDay1 __attribute__ ((packed));
    short dayOfYear1;
    int cmdAz1 __attribute__ ((packed));
    int cmdEl1 __attribute__ ((packed));

    int timeOfDay2 __attribute__ ((packed));
    short dayOfYear2;
    int cmdAz2 __attribute__ ((packed));
    int cmdEl2 __attribute__ ((packed));

    int timeOfDay3 __attribute__ ((packed));
    short dayOfYear3;
    int cmdAz3 __attribute__ ((packed));
    int cmdEl3 __attribute__ ((packed));

    int timeOfDay4 __attribute__ ((packed));
    short dayOfYear4;
    int cmdAz4 __attribute__ ((packed));
    int cmdEl4 __attribute__ ((packed));

    int timeOfDay5 __attribute__ ((packed));
    short dayOfYear5;
    int cmdAz5 __attribute__ ((packed));
    int cmdEl5 __attribute__ ((packed));

    int timeOfDay6 __attribute__ ((packed));
    short dayOfYear6;
    int cmdAz6 __attribute__ ((packed));
    int cmdEl6 __attribute__ ((packed));

    int timeOfDay7 __attribute__ ((packed));
    short dayOfYear7;
    int cmdAz7 __attribute__ ((packed));
    int cmdEl7 __attribute__ ((packed));

    int timeOfDay8 __attribute__ ((packed));
    short dayOfYear8;
    int cmdAz8 __attribute__ ((packed));
    int cmdEl8 __attribute__ ((packed));

    int timeOfDay9 __attribute__ ((packed));
    short dayOfYear9;
    int cmdAz9 __attribute__ ((packed));
    int cmdEl9 __attribute__ ((packed));

    int timeOfDay10 __attribute__ ((packed));
    short dayOfYear10;
    int cmdAz10 __attribute__ ((packed));
    int cmdEl10 __attribute__ ((packed));

    int timeOfDay11 __attribute__ ((packed));
    short dayOfYear11;
    int cmdAz11 __attribute__ ((packed));
    int cmdEl11 __attribute__ ((packed));

    int timeOfDay12 __attribute__ ((packed));
    short dayOfYear12;
    int cmdAz12 __attribute__ ((packed));
    int cmdEl12 __attribute__ ((packed));

    int timeOfDay13 __attribute__ ((packed));
    short dayOfYear13;
    int cmdAz13 __attribute__ ((packed));
    int cmdEl13 __attribute__ ((packed));

    int timeOfDay14 __attribute__ ((packed));
    short dayOfYear14;
    int cmdAz14 __attribute__ ((packed));
    int cmdEl14 __attribute__ ((packed));

    short checksum;
    char etx;
} acuAzElProgTraj; 
