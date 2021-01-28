/*
 
-Header_File SpiceZpl.h ( CSPICE platform macros )
 
-Abstract
 
   Define macros identifying the host platform for which this
   version of CSPICE is targeted.
 
-Copyright
 
   Copyright (1999), California Institute of Technology.
   U.S. Government sponsorship acknowledged.
 
-Required_Reading
 
   None.
 
-Literature_References
 
   None.
 
-Particulars
 
   This header file defines macros that enable CSPICE code to be
   compiled conditionally based on the identity of the host platform.
 
   The macros defined here ARE visible in the macro name space of
   any file that includes SpiceUsr.h.  The names are prefixed with
   the string CSPICE_ to help prevent conflicts with macros defined
   by users' applications.
 
-Author_and_Institution
 
   N.J. Bachman       (JPL)
   B.V. Semenov       (JPL)
   E.D. Wright        (JPL)
 
-Version
 
   -CSPICE Version 1.1.0, 27-JUL-2002 (BVS)
 
      Updated for MAC-OSX environment.
 
   -CSPICE Version 1.0.0, 26-FEB-1999 (NJB) (EDW)
 
-Index_Entries
 
   platform ID macros for CSPICE
 
*/
 
 
#ifndef  HAVE_PLATFORM_MACROS_H
#define HAVE_PLATFORM_MACROS_H
 
 
   #define   CSPICE_PC_LINUX
 
#endif
 
