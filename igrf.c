/****************************************************************************/
/*                                                                          */
/*     NGDC's Geomagnetic Field Modeling software for the IGRF and WMM      */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*     Disclaimer: This program has undergone limited testing. It is        */
/*     being distributed unoffically. The National Geophysical Data         */
/*     Center does not guarantee it's correctness.                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*     Version 7.0:                                                         */
/*     - input file format changed to                                       */
/*            -- accept new DGRF2005 coeffs with 0.01 nT precision          */
/*            -- make sure all values are separated by blanks               */
/*            -- swapped n and m: first is degree, second is order          */
/*     - corrected feet to km conversion factor                             */
/*     Thanks to all who provided bug reports and suggested fixes           */
/*                                                                          */
/*                                          Stefan Maus Jan-25-2010         */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*     Version 6.1:                                                         */
/*     Included option to read coordinates from a file and output the       */
/*     results to a new file, repeating the input and adding columns        */
/*     for the output                                                       */
/*                                          Stefan Maus Jan-31-2008         */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*     Version 6.0:                                                         */
/*     Bug fixes for the interpolation between models. Also added warnings  */
/*     for declination at low H and corrected behaviour at geogr. poles.    */
/*     Placed print-out commands into separate routines to facilitate       */
/*     fine-tuning of the tables                                            */
/*                                          Stefan Maus Aug-24-2004         */
/*                                                                          */
/****************************************************************************/

#include <math.h>
#include <float.h>
#include "vector.h"
#include "igrfCoeffs.h"

#define NaN log(-1.0)
#define FT2KM (1.0/0.0003048)
#define PI 3.141592654

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define RECL 81

#define MAXINBUFF RECL+14

/** Max size of in buffer **/

#define MAXREAD MAXINBUFF-2
/** Max to read 2 less than total size (just to be safe) **/

#define MAXMOD 30
/** Max number of models in a file **/

#define PATH MAXREAD
/** Max path and filename length **/

#define MAXDEG 13
#define MAXCOEFF (MAXDEG*(MAXDEG+2))
double mag_coeff[MAXCOEFF];                   //Computed coefficients

/****************************************************************************/
/*                                                                          */
/*                           Subroutine extrapsh                            */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*     Extrapolates linearly a spherical harmonic model with a              */
/*     rate-of-change model.                                                */
/*                                                                          */
/*     Input:                                                               */
/*           date     - date of resulting model (in decimal year)           */
/*          igrf_date - date of base model                                  */
/*          igrf_ord  - maximum degree and order of base model              */
/*        igrf_coeffs - Schmidt quasi-normal internal spherical             */
/*                      harmonic coefficients of base model                 */
/*           sv_ord   - maximum degree and order of rate-of-change model    */
/*           igrf_sv  - Schmidt quasi-normal internal spherical             */
/*                      harmonic coefficients of rate-of-change model       */
/*                                                                          */
/*     Output:                                                              */
/*        mag_coeff - Schmidt quasi-normal internal spherical               */
/*                    harmonic coefficients                                 */
/*           nmax   - maximum degree and order of resulting model           */
/*                                                                          */
/*     FORTRAN                                                              */
/*           A. Zunde                                                       */
/*           USGS, MS 964, box 25046 Federal Center, Denver, CO.  80225     */
/*                                                                          */
/*     C                                                                    */
/*           C. H. Shaffer                                                  */
/*           Lockheed Missiles and Space Company, Sunnyvale CA              */
/*           August 16, 1988                                                */
/*                                                                          */
/****************************************************************************/


int extrapsh(double date){
  int   nmax;
  int   k, l;
  int   i;
  double factor;
  //# of years to extrapolate
  factor = date - igrf_date;
  //check for equal degree
  if (igrf_ord == sv_ord){
      k =  igrf_ord * (igrf_ord + 2);
      nmax = igrf_ord;
  }else{
      //check if reference is bigger
      if (igrf_ord > sv_ord){
          k = sv_ord * (sv_ord + 2);
          l = igrf_ord * (igrf_ord + 2);
          //copy extra elements unchanged
          for ( i = k ; i < l; ++i){
              mag_coeff[i] = igrf_coeffs[i];
          }
          //maximum degree of model
          nmax = igrf_ord;
      }else{
          k = igrf_ord * (igrf_ord + 2);
          l = sv_ord * (sv_ord + 2);
          //put in rate of change for extra elements?
          for(i=k;i<l;++i){
            mag_coeff[i] = factor * igrf_sv[i];
          }
          nmax = sv_ord;
        }
    }
    for ( i = 0; i < k; ++i){
        mag_coeff[i] = igrf_coeffs[i] + factor * igrf_sv[i];
    }
    return nmax;
}


/****************************************************************************/
/*                                                                          */
/*                           Subroutine shval3                              */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*     Calculates field components from spherical harmonic (sh)             */
/*     models.                                                              */
/*                                                                          */
/*     Input:                                                               */
/*           latitude  - north latitude, in radians                         */
/*           longitude - east longitude, in radians                         */
/*           elev      - radial distance from earth's center                */
/*           nmax      - maximum degree and order of coefficients           */
/*                                                                          */
/*     Output:                                                              */
/*           x         - northward component                                */
/*           y         - eastward component                                 */
/*           z         - vertically-downward component                      */
/*                                                                          */
/*     based on subroutine 'igrf' by D. R. Barraclough and S. R. C. Malin,  */
/*     report no. 71/1, institute of geological sciences, U.K.              */
/*                                                                          */
/*     FORTRAN                                                              */
/*           Norman W. Peddie                                               */
/*           USGS, MS 964, box 25046 Federal Center, Denver, CO.  80225     */
/*                                                                          */
/*     C                                                                    */
/*           C. H. Shaffer                                                  */
/*           Lockheed Missiles and Space Company, Sunnyvale CA              */
/*           August 17, 1988                                                */
/*                                                                          */
/****************************************************************************/


int shval3(double flat,double flon,double elev,int nmax,VEC *dest){
  const double earths_radius = 6371.2;
  double slat;
  double clat;
  double ratio;
  double aa, bb, cc;
  double rr;
  double fm,fn;
  double sl[MAXCOEFF];
  double cl[MAXCOEFF];
  double p[MAXDEG*(MAXDEG+3)/2];
  double q[MAXDEG*(MAXDEG+3)/2];
  int i,j,k,l,m,n;
  int npq;
  double x,y,z;

  //calculate sin and cos of latitude
  slat = sin(flat);
  clat = cos(flat);
  //prevent divide by zero
  if(clat==0){
    clat=DBL_EPSILON;
  }

  //calculate sin and cos of longitude
  sl[0] = sin(flon);
  cl[0] = cos(flon);
  //initialize coordinates
  x = 0;
  y = 0;
  z = 0;

  //calculate loop iterations
  npq = (nmax * (nmax + 3)) / 2;

  //calculate ratio of earths radius to elevation
  ratio = earths_radius / elev;

  aa = sqrt(3.0);

  //set initial values of p
  p[0] = 2.0 * slat;
  p[1] = 2.0 * clat;
  p[2] = 4.5 * slat * slat - 1.5;
  p[3] = 3.0 * aa * clat * slat;

  //Set initial values of q
  q[0] = -clat;
  q[1] = slat;
  q[2] = -3.0 * clat * slat;
  q[3] = aa * (slat * slat - clat * clat);

  for(k=0,l=1,n=0,m=0; k+1 <= npq;k++,m++){
      if (n <= m){
          m = -1;
          n+= 1;
          rr = pow(ratio,n+2);
          fn = n;
      }
      fm = m+1;
      if (k >= 4){
          if (m+1 == n){
              aa = sqrt(1.0 - 0.5/fm);
              j = k - n ;
              p[k] = (1.0 + 1.0/fm) * aa * clat * p[j-1];
              q[k] = aa * (clat * q[j-1] + slat/fm * p[j-1]);
              sl[m] = sl[m-1] * cl[0] + cl[m-1] * sl[0];
              cl[m] = cl[m-1] * cl[0] - sl[m-1] * sl[0];
          }else{
              aa = sqrt(fn*fn - fm*fm);
              bb = sqrt(((fn - 1.0)*(fn-1.0)) - (fm * fm))/aa;
              cc = (2.0 * fn - 1.0)/aa;
              i = k - n;
              j = k - 2 * n + 1;
              p[k] = (fn + 1.0) * (cc * slat/fn * p[i] - bb/(fn - 1.0) * p[j]);
              q[k] = cc * (slat * q[i] - clat/fn * p[i]) - bb * q[j];
            }
        }
        aa = rr * mag_coeff[l-1];

      if (m == -1){
          x = x + aa * q[k];
          z = z - aa * p[k];
          l+= 1;
      }else{
              bb = rr * mag_coeff[l];
              cc = aa * cl[m] + bb * sl[m];
              x = x + cc * q[k];
              z = z - cc * p[k];
              if (clat > 0){
                  y = y + (aa * sl[m] - bb * cl[m]) *fm * p[k]/((fn + 1.0) * clat);
              }else{
                  y = y + (aa * sl[m] - bb * cl[m]) * q[k] * slat;
              }
              l+= 2;
      }
    }

    dest->c.x=x;
    dest->c.y=y;
    dest->c.z=z;
    //set destination values
    //always returns zero
    return 0;
}
