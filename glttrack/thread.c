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

	sleep(1);

	} /* while */

	pthread_detach(CommandHandlerTID);
	pthread_exit((void *) 0);
}
