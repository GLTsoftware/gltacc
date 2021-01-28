/*

-Header_File SpiceSPK.h ( CSPICE SPK definitions )

-Abstract

   Perform CSPICE definitions to support SPK wrapper interfaces.
            
-Copyright

   Copyright (2002), California Institute of Technology.
   U.S. Government sponsorship acknowledged.

-Required_Reading

   None.
   
-Particulars

   This header defines types that may be referenced in 
   application code that calls CSPICE SPK functions.

      Typedef
      =======
   
         Name                  Description
         ----                  ----------
   
         SpiceSPK18Subtype     Typedef for enum indicating the 
                               mathematical representation used
                               in an SPK type 18 segment.  Possible
                               values and meanings are:

                                S18TP0:
 
                                  Hermite interpolation, 12-
                                  element packets containing 
                                  
                                     x,  y,  z,  dx/dt,  dy/dt,  dz/dt, 
                                     vx, vy, vz, dvx/dt, dvy/dt, dvz/dt 
                   
                                  where x, y, z represent Cartesian
                                  position components and vx, vy, vz
                                  represent Cartesian velocity
                                  components.  Note well:  vx, vy, and
                                  vz *are not necessarily equal* to the
                                  time derivatives of x, y, and z.
                                  This packet structure mimics that of
                                  the Rosetta/MEX orbit file from which
                                  the data are taken.
 
                                  Position units are kilometers,
                                  velocity units are kilometers per
                                  second, and acceleration units are
                                  kilometers per second per second.


                                S18TP1:
  
                                  Lagrange interpolation, 6-
                                  element packets containing 

                                     x,  y,  z,  dx/dt,  dy/dt,  dz/dt
 
                                  where x, y, z represent Cartesian
                                  position components and  vx, vy, vz
                                  represent Cartesian velocity
                                  components.
 
                                  Position units are kilometers;
                                  velocity units are kilometers per
                                  second.
 
-Literature_References

   None.

-Author_and_Institution

   N.J. Bachman       (JPL)
   
-Restrictions

   None.
      
-Version

   -CSPICE Version 1.0.0, 16-AUG-2002 (NJB)  

*/

#ifndef HAVE_SPICE_SPK_H

   #define HAVE_SPICE_SPK_H
   
   
   
   /*
   SPK type 18 subtype codes:
   */
   
   enum _SpiceSPK18Subtype  { S18TP0, S18TP1 };
   

   typedef enum _SpiceSPK18Subtype SpiceSPK18Subtype;
 
#endif

