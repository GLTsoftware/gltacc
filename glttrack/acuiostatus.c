void *ACUiostatus() {

  int i,n = 0;
  char recvBuff[256];
  char sendBuff[256];
  int days,hh,mm;
  int dsm_status,dsm_open_status;
  double hours,minutes,seconds;
  struct sockaddr_in serv_addr;
  ioStatus ioStatusResp;
  acuCmd acuCommand={};

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
  acuCommand.datalength = 0x7;
  acuCommand.checksum = 0x58;
  acuCommand.etx = 0x3;

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

  dsm_status = dsm_write(ACC,"DSM_AZ_MOTOR1_TEMP_D",&azmotor1temp;
  dsm_status = dsm_write(ACC,"DSM_AZ_MOTOR2_TEMP_D",&azmotor2temp;
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR1_TEMP_D",&elmotor1temp;
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR2_TEMP_D",&elmotor2temp;
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR3_TEMP_D",&elmotor3temp;
  dsm_status = dsm_write(ACC,"DSM_EL_MOTOR4_TEMP_D",&elmotor4temp;

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

        usleep(500000);
	} /* while loop */

	pthread_detach(ACUiostatusTID);
	pthread_exit((void *) 0);
}
