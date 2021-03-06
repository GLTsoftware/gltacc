/* This file has PMAC hardware related mappings to DPRAM */
#define VME_STD_ADD                     0xc0000000
#define ACU_DP_BASE                     0x700000
#define PMAC                            VME_STD_ADD + ACU_DP_BASE
#define HOST_FLAG_OFFSET                0x62c
#define PMAC_FLAG_OFFSET                0x6d0
#define PMAC_STRING_LENGTH_OFFSET       0x6d2
#define HOST_STRING_OFFSET              0x630
#define PMAC_STRING_OFFSET              0x6d4

/* Nimesh A. Patel */
#define ANTENNA_AXIS_LIMITS      0x1106
#define A_AZPOSLIM_FLAG 	  0x1106  /* m124->X:$D441,0 */
#define A_AZPOSLIM_FLAG_MASK 	 0x00000001 
#define A_AZPOSLIM_FLAG_OFFSET 	 0x0 
#define A_AZNEGLIM_FLAG 	  0x1106  /* m125->X:$D441,1 */
#define A_AZNEGLIM_FLAG_MASK 	 0x00000002 
#define A_AZNEGLIM_FLAG_OFFSET 	 0x1 
#define A_MTRFAULT_AZ1_FLAG 	  0x1106  /* m126->X:$D441,7 */
#define A_MTRFAULT_AZ1_FLAG_MASK 	 0x00000080 
#define A_MTRFAULT_AZ1_FLAG_OFFSET 	 0x7 
#define A_MTRTEMP_AZL_FLAG 	  0x110a  /* m127->X:$D442,3 */
#define A_MTRTEMP_AZL_FLAG_MASK 	 0x00000008 
#define A_MTRTEMP_AZL_FLAG_OFFSET 	 0x3 
#define A_MTRCURRETN_AZL_FLAG 	  0x1112  /* m128->X:$D444,0 */
#define A_MTRCURRETN_AZL_FLAG_MASK 	 0x00000001 
#define A_MTRCURRETN_AZL_FLAG_OFFSET 	 0x0 
#define A_AMPENABLE_AZ1 	  0x1106  /* m137->X:$D441,10 */
#define A_AMPENABLE_AZ1_MASK 	 0x00000400 
#define A_AMPENABLE_AZ1_OFFSET 	 0xa 
#define A_LAPCTRPOS 	  0x1106  /* m176->X:$D441,4 */
#define A_LAPCTRPOS_MASK 	 0x00000010 
#define A_LAPCTRPOS_OFFSET 	 0x4 
#define A_LAPCTRNEG 	  0x1106  /* m177->X:$D441,5 */
#define A_LAPCTRNEG_MASK 	 0x00000020 
#define A_LAPCTRNEG_OFFSET 	 0x5 
#define A_ELPOSLIM_FLAG 	  0x1106  /* m224->X:$D441,2 */
#define A_ELPOSLIM_FLAG_MASK 	 0x00000004 
#define A_ELPOSLIM_FLAG_OFFSET 	 0x2 
#define A_ELNEGLIM_FLAG 	  0x1106  /* m225->X:$D441,3 */
#define A_ELNEGLIM_FLAG_MASK 	 0x00000008 
#define A_ELNEGLIM_FLAG_OFFSET 	 0x3 
#define A_MTRFAULT_AZ2_FLAG 	  0x1106  /* m226->X:$D441,8 */
#define A_MTRFAULT_AZ2_FLAG_MASK 	 0x00000100 
#define A_MTRFAULT_AZ2_FLAG_OFFSET 	 0x8 
#define A_MTRTEMP_AZR_FLAG 	  0x110a  /* m227->X:$D442,4 */
#define A_MTRTEMP_AZR_FLAG_MASK 	 0x00000010 
#define A_MTRTEMP_AZR_FLAG_OFFSET 	 0x4 
#define A_MTRCURRENT_AZR_FLAG 	  0x110a  /* m228->X:$D442,1 */
#define A_MTRCURRENT_AZR_FLAG_MASK 	 0x00000002 
#define A_MTRCURRENT_AZR_FLAG_OFFSET 	 0x1 
#define A_AMPENABLE_AZ2 	  0x1106  /* m237->X:$D441,11 */
#define A_AMPENABLE_AZ2_MASK 	 0x00000800 
#define A_AMPENABLE_AZ2_OFFSET 	 0xb 
#define A_MTRFAULT_EL_FLAG 	  0x1106  /* m326->X:$D441,9 */
#define A_MTRFAULT_EL_FLAG_MASK 	 0x00000200 
#define A_MTRFAULT_EL_FLAG_OFFSET 	 0x9 
#define A_MTRTEMP_EL_FLAG 	  0x110a  /* m327->X:$D442,5 */
#define A_MTRTEMP_EL_FLAG_MASK 	 0x00000020 
#define A_MTRTEMP_EL_FLAG_OFFSET 	 0x5 
#define A_MTRCURRENT_EL_FLAG 	  0x110a  /* m328->X:$D442,2 */
#define A_MTRCURRENT_EL_FLAG_MASK 	 0x00000004 
#define A_MTRCURRENT_EL_FLAG_OFFSET 	 0x2 
#define A_AMPENABLE_EL 	  0x1106  /* m337->X:$D441,12 */
#define A_AMPENABLE_EL_MASK 	 0x00001000 
#define A_AMPENABLE_EL_OFFSET 	 0xc 
#define A_PLS_LIM_PADID 	  0x110e  /* m424->X:$D443,0,8,u */
#define A_PLS_LIM_PADID_MASK 	 0x000000ff 
#define A_PLS_LIM_PADID_OFFSET 	 0x0 
#define A_PLS_AZLIM_FLAG 	  0x110e  /* m425->X:$D443,9 */
#define A_PLS_AZLIM_FLAG_MASK 	 0x00000200 
#define A_PLS_AZLIM_FLAG_OFFSET 	 0x9 
#define A_PLS_ELLIM_FLAG 	  0x110e  /* m426->X:$D443,10 */
#define A_PLS_ELLIM_FLAG_MASK 	 0x00000400 
#define A_PLS_ELLIM_FLAG_OFFSET 	 0xa 
#define A_PLS_VELLIM_AZ 	  0x110e  /* m427->X:$D443,11 */
#define A_PLS_VELLIM_AZ_MASK 	 0x00000800 
#define A_PLS_VELLIM_AZ_OFFSET 	 0xb 
#define A_PLS_VELLIM_EL 	  0x110e  /* m428->X:$D443,12 */
#define A_PLS_VELLIM_EL_MASK 	 0x00001000 
#define A_PLS_VELLIM_EL_OFFSET 	 0xc 
#define A_PLS_ACCNLIM_AZ 	  0x110e  /* m429->X:$D443,13 */
#define A_PLS_ACCNLIM_AZ_MASK 	 0x00002000 
#define A_PLS_ACCNLIM_AZ_OFFSET 	 0xd 
#define A_PLS_ACCNLIM_EL 	  0x110e  /* m430->X:$D443,14 */
#define A_PLS_ACCNLIM_EL_MASK 	 0x00004000 
#define A_PLS_ACCNLIM_EL_OFFSET 	 0xe 
#define A_PLS_LAPCTR_CHK 	  0x110e  /* m431->X:$D443,15 */
#define A_PLS_LAPCTR_CHK_MASK 	 0x00008000 
#define A_PLS_LAPCTR_CHK_OFFSET 	 0xf 
#define A_PLS_ENCODER_CHK 	  0x110e  /* m432->X:$D443,16 */
#define A_PLS_ENCODER_CHK_MASK 	 0x00010000 
#define A_PLS_ENCODER_CHK_OFFSET 	 0x10 
#define A_PLC_STATUS 	  0x1400  /* m756->DP:$D500 */
#define A_PLC_STATUS_L 	  0x1400  /* m756->DP:$D500 */
#define A_PLC_STATUS_H 	  0x1402  /* m756->DP:$D500 */
#define A_AZIL_MOT_TMP 	  0x1080  /* m981->Y:$D420,0,16,S */
#define A_AZIL_MOT_CUR 	  0x1082  /* m982->X:$D420,0,16,S */
#define A_AZIR_MOT_TMP 	  0x1084  /* m983->Y:$D421,0,16,S */
#define A_AZIR_MOT_CUR 	  0x1086  /* m984->X:$D421,0,16,S */
#define A_EL_MOT_TMP 	  0x1088  /* m993->Y:$D422,0,16,S */
#define A_EL_MOT_CUR 	  0x108a  /* m994->X:$D422,0,16,S */
#define A_JOPT_OUT 	  0x1940  /* m1017->Y:$D650,0,16 */
#define A_JOPT_IN 	  0x1942  /* m1018->X:$D650,0,16 */
