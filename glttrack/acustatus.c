void *ACUstatus() {

  int n = 0;
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
  acuCmd acuCommand;


  acuCommand.stx = 0x2;
  acuCommand.id = 0x71;
  acuCommand.datalength = 0x7;
  acuCommand.checksum = 0x78;
  acuCommand.etx = 0x3;

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
  hours = acuStatusResp.timeOfDay/3600000.;
  hh = (int)hours;
  minutes = (hours-hh)*60.;
  mm = (int) minutes;
  seconds = (minutes-mm)*60.;
*/
  printf ("ACU Time: (day, hh:mm:ss.sss):  %d %02d:%02d:%02.3f\n",acuStatusResp.dayOfYear,hh,mm,seconds);
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

  dsm_status = dsm_write(ACC,"DSM_AZ_POSN_DEG_D",&az);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_AZ_POSN_DEG dsm_status=%d\n",dsm_status);
  }

  dsm_status = dsm_write(ACC,"DSM_EL_POSN_DEG_D",&el);

  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_EL_POSN_DEG dsm_status=%d\n",dsm_status);
  }

  dsm_status = dsm_write(ACC,"DSM_AZ_ACU_CMD_POSN_DEG_D",&acuCmdAz);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_AZ_ACU_CMD_POSN_DEG dsm_status=%d\n",dsm_status);
  }

  dsm_status = dsm_write(ACC,"DSM_EL_ACU_CMD_POSN_DEG_D",&acuCmdEl);

  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! DSM_EL_ACU_CMD_POSN_DEG dsm_status=%d\n",dsm_status);
  }

/*
  az_tracking_error = (float)(commanded_az-az);
  el_tracking_error = (float)(commanded_el-el);

  dsm_status = dsm_write(ACC,"DSM_AZ_TRACKING_ERROR_F",&az_tracking_error);
  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! dsm_status=%d\n",dsm_status);
  }

  dsm_status = dsm_write(ACC,"DSM_EL_TRACKING_ERROR_F",&el_tracking_error);

  if (dsm_status != DSM_SUCCESS) {
  printf("Warning: DSM write failed! dsm_status=%d\n",dsm_status);
  }
*/


/*
   DSM_ACU_SYSTEMGS_C6
   DSM_ACU_SERVO_STATUS_AZ_C2
   DSM_ACU_SERVO_STATUS_EL_C2
   DSM_ACU_ERROR_MESSAGE_C256
   DSM_ACU_DAYOFYEAR_L
   DSM_ACU_HOUR_L
   DSM_ACU_MINUTE_L
   DSM_ACU_SECONDS_D

*/


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
  printf("ACU refuses the command...reason:");
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

        sleep(1);
	} /* while */

	pthread_detach(ACUstatusTID);
	pthread_exit((void *) 0);
}
