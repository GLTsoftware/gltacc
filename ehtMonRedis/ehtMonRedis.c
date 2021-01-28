#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <hiredis.h>
#include <arpa/inet.h>
#include "dsm.h"


#define DSM_HOST "gltobscon"
#define PRINT_DSM_ERRORS 0
#define UPDATERATE 15

int call_dsm_read(char *machine, char *variable, void *ptr, time_t *tstamp); 
char* trimwhitespace(char *str_base); 
 
int main(void) {
  int dsm_status;
  time_t timeStamp,timestamp;
  char redisData[2048];
  redisContext *c;
  redisReply *reply;
  struct timeval timeout = {2,500000 }; /* 1.5 seconds  for redis */
 /* weather variables from DSM */
  float tau;
  float tsysAmbLeft,tsysAmbRight;
  short acuModeAz;
  char source[34],Source[34];
  double az_actual_corrected,el_actual_disp,ra_disp,dec_disp;
  char antennaMode[100];
  int rms;

while(1) {  
/* time stamp */
  timeStamp = time(NULL);
        /* initializing DSM */
        dsm_status=dsm_open();
        if(dsm_status != DSM_SUCCESS) {
                dsm_error_message(dsm_status,"dsm_open()");
                fprintf(stderr,"Could not open distributed shared  memory.");
                fprintf(stderr,"Check if dsm is running.");
                exit(-1);
        }

rms = dsm_read(DSM_HOST,"DSM_ACU_MODE_STATUS_AZ_S",&acuModeAz,&timestamp);
     rms=call_dsm_read(DSM_HOST,"DSM_RA_APP_HR_D", &ra_disp,&timestamp);
          rms=call_dsm_read(DSM_HOST,"DSM_DEC_APP_DEG_D",&dec_disp,&timestamp);

rms=call_dsm_read(DSM_HOST,"DSM_SOURCE_C34",source,&timestamp);
        rms=call_dsm_read(DSM_HOST,"DSM_AZ_POSN_DEG_D",&az_actual_corrected,&timestamp);
          rms=call_dsm_read(DSM_HOST,"DSM_EL_POSN_DEG_D",&el_actual_disp,&timestamp);
  rms = dsm_read(DSM_HOST,"DSM_RADIOMETER_TAU_F",&tau,&timestamp);
  rms = dsm_read(DSM_HOST,"DSM_TSYS_AMB_LEFT_F",&tsysAmbLeft,&timestamp);
  rms = dsm_read(DSM_HOST,"DSM_TSYS_AMB_RIGHT_F",&tsysAmbRight,&timestamp);
          if (rms!=DSM_SUCCESS) printf("dsm_read error, return code=%d\n",rms);
strcpy(Source,trimwhitespace(source));

     strcpy(antennaMode,"Unknown");
      if(acuModeAz==0x1) strcpy(antennaMode,"Stop");
        if(acuModeAz==0x2) strcpy(antennaMode,"Preset");
        if(acuModeAz==0x3) strcpy(antennaMode,"Track");
        if(acuModeAz==0x6) strcpy(antennaMode,"Stow");
        if(acuModeAz==0xe) strcpy(antennaMode,"Stow");

if(dsm_status!=DSM_SUCCESS) dsm_error_message(dsm_status,"dsm_read()");


        dsm_close();
   /* send data to redis server */
  c = redisConnectWithTimeout("192.168.1.11",6379,timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
   /* exit(1); */
    }
  
sprintf(redisData,"ZADD ehtVLBImonData %d \"{'timestamp':%d,'antennaMode':\'%s\','appRA':%f,'appDEC':%f,'source':\'%s\','az':%f,'el':%f,'tau':%f,'tsys1':%f,'tsys2':%f}\"", (int)timeStamp,(int)timeStamp,antennaMode,ra_disp,dec_disp,Source,az_actual_corrected,el_actual_disp,tau,tsysAmbLeft,tsysAmbRight);

/*
printf("ZADD ehtVLBImonData %d \"{'timestamp':%d,'antennaMode':%s,'appRA':%f,'appDEC':%f,'source':%s,'az':%f,'el':%f,'tau':%f}\"", (int)timeStamp,(int)timeStamp,antennaMode,ra_disp,dec_disp,Source,az_actual_corrected,el_actual_disp,tau);
*/

printf("redis data string: %s\n",redisData);

    reply = redisCommand(c,redisData);
    printf("ZADD: %s\n",reply->str);
    freeReplyObject(reply);
    
    redisFree(c);
sleep(UPDATERATE);
} /* while loop */
    
  return 0;
}

int call_dsm_read(char *machine, char *variable, void *ptr, time_t *tstamp) {
  char buf[256];
  int rms;
  rms = dsm_read(machine,variable,ptr,tstamp);
  if (rms != DSM_SUCCESS) {
    if (PRINT_DSM_ERRORS) {
      sprintf(buf,"dsm_read - %s",variable);
      dsm_error_message(rms, buf);
    }
  }
  return(rms);
}

char* trimwhitespace(char *str_base) {
    char* buffer = str_base;
    while((buffer = strchr(str_base, ' '))) {
        strcpy(buffer, buffer+1);
    }

    return str_base;
}
