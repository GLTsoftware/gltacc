/* (C) 1998 Red Hat Software, Inc. -- Licensing details are in the COPYING
   file accompanying smapopt source distributions, available from 
   ftp://ftp.redhat.com/pub/code/smapopt */

#ifndef H_SMAPOPT
#define H_SMAPOPT

#include <stdio.h>			/* for FILE * */

#define SMAPOPT_OPTION_DEPTH    10
#define SMAPOPT_MAX_ANTENNAS     10
#define SMAPOPT_MAX_RECEIVERS     2
#define SMAPOPT_MAX_CRATES       12

#define SMAPOPT_ARG_NONE	  0
#define SMAPOPT_ARG_STRING	  1
#define SMAPOPT_ARG_INT		  2
#define SMAPOPT_ARG_LONG	  3
#define SMAPOPT_ARG_INCLUDE_TABLE 4	/* arg points to table */
#define SMAPOPT_ARG_CALLBACK	  5	/* table-wide callback... must be
					   set first in table; arg points 
					   to callback, descrip points to 
					   callback data to pass */
#define SMAPOPT_ARG_DOUBLE        6     /* double precision float */
#define SMAPOPT_ARG_TIME          7     /* Time, decimal or HH:MM:SS.SS */
#define SMAPOPT_ARG_DEC           8     /* Angle between -90.0 and +90.0 */
#define SMAPOPT_ARG_ANGLE         9     /* Angle */
#define SMAPOPT_ARG_ANTENNA      10     /* Valid antenna number */
#define SMAPOPT_ARG_BLOCK        11     /* Valid block number */
#define SMAPOPT_ARG_CHUNK        12     /* Valid chunk number */
#define SMAPOPT_ARG_CRATE        13     /* Valid crate number */
#define SMAPOPT_ARG_PAD          14     /* Valid pad number */
#define SMAPOPT_ARG_RECEIVER     15     /* Valid receiver */
#define SMAPOPT_ARG_ANTENNAS     16     /* Array of antennas */
#define SMAPOPT_ARG_CRATES       17     /* Array of correlator crates */
#define SMAPOPT_ARG_DATETIME     18     /* Date & time string - return Unix time */
#define SMAPOPT_ARG_DATA_TYPE    19     /* Type of data (science, engineering, pointing, etc) */
#define SMAPOPT_ARG_INSERT       20     /* Receiver insert (A1, A2 ...) */
#define SMAPOPT_ARG_MASK 0x0000FFFF
#define SMAPOPT_ARGFLAG_ONEDASH	0x80000000  /* allow -longoption */
#define SMAPOPT_ARGFLAG_DOC_HIDDEN 0x40000000  /* don't show in help/usage */
#define SMAPOPT_CBFLAG_PRE	0x80000000  /* call the callback before parse */
#define SMAPOPT_CBFLAG_POST	0x40000000  /* call the callback after parse */
#define SMAPOPT_CBFLAG_INC_DATA	0x20000000  /* use data from the include line,
					       not the subtable */

#define SMAPOPT_ERROR_NOARG	-10
#define SMAPOPT_ERROR_BADOPT	-11
#define SMAPOPT_ERROR_OPTSTOODEEP -13
#define SMAPOPT_ERROR_BADQUOTE	-15	/* only from smapoptParseArgString() */
#define SMAPOPT_ERROR_ERRNO	-16	/* only from smapoptParseArgString() */
#define SMAPOPT_ERROR_BADNUMBER	-17
#define SMAPOPT_ERROR_OVERFLOW	-18
#define SMAPOPT_ERROR_BADVALUE  -19

#define SMAPOPT_INSERT_A1 (1)
#define SMAPOPT_INSERT_A2 (2)
#define SMAPOPT_INSERT_B1 (3)
#define SMAPOPT_INSERT_B2 (4)
#define SMAPOPT_INSERT_C  (5)
#define SMAPOPT_INSERT_D  (6)
#define SMAPOPT_INSERT_E  (7)
#define SMAPOPT_INSERT_F  (8)

/* smapoptBadOption() flags */
#define SMAPOPT_BADOPTION_NOALIAS  (1 << 0)  /* don't go into an alias */

/* smapoptGetContext() flags */
#define SMAPOPT_CONTEXT_NO_EXEC	(1 << 0)  /* ignore exec expansions */
#define SMAPOPT_CONTEXT_KEEP_FIRST	(1 << 1)  /* pay attention to argv[0] */
#define SMAPOPT_CONTEXT_POSIXMEHARDER (1 << 2) /* options can't follow args */
#define SMAPOPT_CONTEXT_NOLOG (1 << 3) /* Don't log command in the SMAsh log */
#define SMAPOPT_CONTEXT_EXPLAIN (1 << 4) /* Context table contains eplain string */
struct smapoptProject {         /* A structure to hold the information con-  */
                                /* cerning the project associated with the   */
                                /* process which executed the SMAsh command  */
  int projectID;                /* Project ID number, 0 if non project       */
                                /* Resource flags: (1 means allocated)       */
  int antennas[SMAPOPT_MAX_ANTENNAS+1]; /* Allocated antennas                */
  int receivers[SMAPOPT_MAX_RECEIVERS+1]; /* Allocated receivers             */
  int crates[SMAPOPT_MAX_CRATES+1]; /* Allocated corellator crates           */
  int correlator;
};

struct smapoptOption {
    const char * longName;	/* may be NULL */
    char shortName;		/* may be '\0' */
    int argInfo;
    void * arg;			/* depends on argInfo */
    int val;			/* 0 means don't return, just update flag */
    char * descrip;		/* description for autohelp -- may be NULL */
    char * argDescrip;		/* argument description for autohelp */
};

struct smapoptAlias {
    char * longName;		/* may be NULL */
    char shortName;		/* may be '\0' */
    int argc;
    char ** argv;		/* must be free()able */
};

extern struct smapoptOption smapoptHelsmapoptions[];
#define SMAPOPT_AUTOHELP { NULL, '\0', SMAPOPT_ARG_INCLUDE_TABLE, smapoptHelsmapoptions, \
			0, "Help options", NULL },

typedef struct smapoptContext_s * smapoptContext;
#ifndef __cplusplus
typedef struct smapoptOption * smapoptOption;
typedef struct smapoptProject * smapoptProject;
#endif

enum smapoptCallbackReason { SMAPOPT_CALLBACK_REASON_PRE, 
			  SMAPOPT_CALLBACK_REASON_POST,
			  SMAPOPT_CALLBACK_REASON_OPTION };
typedef void (*smapoptCallbackType)(smapoptContext con, 
				 enum smapoptCallbackReason reason,
			         const struct smapoptOption * opt,
				 const char * arg, void * data);

smapoptContext smapoptGetContext(char * name, int argc, char ** argv, 
			   const struct smapoptOption * options, int flags);
smapoptContext smapoptGetProjectContext(char * name, int argc, char ** argv, 
			   const struct smapoptOption * options, int flags,
			   struct smapoptProject *project);
void smapoptResetContext(smapoptContext con);

/* returns 'val' element, -1 on last item, SMAPOPT_ERROR_* on error */
int smapoptGetNextOpt(smapoptContext con);
/* returns NULL if no argument is available */
char * smapoptGetOptArg(smapoptContext con);
/* returns NULL if no more options are available */
char * smapoptGetArg(smapoptContext con);
char * smapoptPeekArg(smapoptContext con);
char ** smapoptGetArgs(smapoptContext con);
/* returns the option which caused the most recent error */
char * smapoptBadOption(smapoptContext con, int flags);
void smapoptFreeContext(smapoptContext con);
int smapoptStuffArgs(smapoptContext con, char ** argv);
int smapoptAddAlias(smapoptContext con, struct smapoptAlias alias, int flags);
int smapoptReadConfigFile(smapoptContext con, char * fn);
/* like above, but reads /etc/smapopt and $HOME/.smapopt along with environment 
   vars */
int smapoptReadDefaultConfig(smapoptContext con, int useEnv);
/* argv should be freed -- this allows ', ", and \ quoting, but ' is treated
   the same as " and both may include \ quotes */
int smapoptParseArgvString(char * s, int * argcPtr, char *** argvPtr);
const char * smapoptStrerror(const int error);
void smapoptSetExecPath(smapoptContext con, const char * path, int allowAbsolute);
void smapoptPrintHelp(smapoptContext con, FILE * f, int flags);
void smapoptPrintUsage(smapoptContext con, FILE * f, int flags);
void smapoptSetOtherOptionHelp(smapoptContext con, const char * text);
const char * smapoptGetInvocationName(smapoptContext con);

#endif
