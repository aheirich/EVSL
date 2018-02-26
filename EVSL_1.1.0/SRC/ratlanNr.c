#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include <complex.h>
#include "def.h"
#include "blaslapack.h"
#include "struct.h"
#include "internal_proto.h"
/**
 * @file ratlanNr.c
 * @brief Rational Filtered no-restart Lanczos
 */
/**
 * if filter the initial vector
 */ 
#define FILTER_VINIT 1

/**-----------------------------------------------------------------------
 *  @brief Rational filtering Lanczos process [NON-restarted version]
 *
 *  @param intv   an array of length 4 
 *          [intv[0], intv[1]] is the interval of desired eigenvalues
 *          [intv[2], intv[3]] is the global interval of all eigenvalues
 *          it must contain all eigenvalues of A
 *  
 *  @param maxit  max Num of outer Lanczos steps  allowed --[max dim of Krylov 
 *          subspace]
 *  
 *  @param tol    tolerance  for    convergence.  The  code  uses   a  stopping
 *          criterion  based   on  the  convergence  of   the  restricted
 *          trace. i.e.,  the sum of the  eigenvalues of T_k that  are in
 *          the desired interval. This test  is rather simple since these
 *          eigenvalues are above `bar'.  We want the relative
 *          error on  this restricted  trace to be  less than  tol.  Note
 *          that the test  performed on filtered matrix only  - *but* the
 *          actual residual norm associated with the original matrix A is
 *          returned
 *  
 *  @param vinit  initial  vector for Lanczos -- [optional]
 * 
 * 
 *  @warning RatLanNr() Modifies the following variables:
 *
 *  @param[out] rat      A struct containing the rational filter
 *  @param[out] nevOut   Number of eigenvalues/vectors computed
 *  @param[out] Wo       A set of eigenvectors  [n x nevOut matrix]
 *  @param[out] lamo     Associated eigenvalues [nevOut x 1 vector]
 *  @param[out] reso     Associated residual norms [nev x 1 vector]
 * @param[out] fstats   File stream which stats are printed to
 *
 * ------------------------------------------------------------ */
int RatLanNr(double *intv, int maxit, double tol, double *vinit, 
             ratparams *rat, int *nevOut, double **lamo, double **Wo, 
             double **reso, FILE *fstats) {
  //-------------------- to report timings/
  double tall, tm1 = 0.0, tt;
  tall = evsl_timer();
  const int ifGenEv = evsldata.ifGenEv;
  double tr0, tr1;
  double *y, flami; 
  int i, k, kdim;
  // handle case where fstats is NULL. Then no output. Needed for openMP.
  int do_print = 1;   
  if (fstats == NULL){
    do_print = 0;
  }  
  /*--------------------   frequently used constants  */
  char cN = 'N';   
  int one = 1;
  double done = 1.0, dzero = 0.0;
  /*--------------------   Ntest = when to start testing convergence */
  int Ntest = 30; 
  /*--------------------   how often to test */
  int cycle = 20; 
  /* size of the matrix */
  int n = evsldata.n;
  /* max num of its */
  maxit = evsl_min(n, maxit);
  /*-------------------- Caveat !!!: To prevent integer overflow, we save 
   *                     another size_t version of n
   *                     Note n itself may not overflow but something like 
   *                     (maxit * n) is much more likely 
   *                     When using size_t n, in all the multiplications 
   *                     integer is promoted to size_t */
  size_t n_l = n;
  /*-------------------- Rational filter with pole at ((a+b)/2,(b-a)/2) with 
    multiplicity pow, bar value equals 1/2        */
  /*-------------------- a, b, used for testing only at end */
  double bar = 0.5;
  if (check_intv(intv, fstats) < 0) {
    *nevOut = 0;
    *lamo = NULL; *Wo = NULL; *reso = NULL;
    return 0;
  }
  double aa = intv[0];
  double bb = intv[1];
  //int deg = rat->pow; // multiplicity of the pole
  /*-----------------------------------------------------------------------* 
   * *Non-restarted* Lanczos iteration 
   *-----------------------------------------------------------------------*/
  if (do_print) {
    fprintf(fstats, " ** Rat-LanNr \n");
  }
  /*-------------------- Lanczos vectors V_m and tridiagonal matrix T_m */
  double *V, *dT, *eT, *Z;
  Malloc(V, n_l*(maxit+1), double);
  if (ifGenEv) {
    /* storage for Z = B * V */
    Malloc(Z, n_l*(maxit+1), double);
  } else {
    /* Z and V are the same */
    Z = V;
  }
  /*-------------------- diag. subdiag of Tridiagional matrix */
  Malloc(dT, maxit, double);
  Malloc(eT, maxit, double);
  double *Rvec, *Lam, *res, *EvalT, *EvecT;
  /*-------------------- Lam, Rvec: the converged (locked) Ritz values vecs*/
  Malloc(Lam,   maxit, double);       // holds computed Ritz values
  Malloc(res,   maxit, double);       // residual norms (w.r.t. ro(A))
  Malloc(EvalT, maxit, double);       // eigenvalues of tridia. matrix  T
  /*-------------------- nev = current number of converged e-pairs 
                         nconv = converged eigenpairs from looking at Tk alone */
  int nev, nconv = 0;
  /*-------------------- u  is just a pointer. wk == work space */
  double *u, *wk, *w2, *vrand = NULL;
  size_t wk_size = ifGenEv ? 6*n_l : 4*n_l;
  Malloc(wk, wk_size, double);
  w2 = wk + n;
  /*-------------------- copy initial vector to Z(:,1) */
#if FILTER_VINIT
  /* Filter the initial vector */
  RatFiltApply(n, rat, vinit, V, wk);
  Malloc(vrand, n, double);
  if(ifGenEv){
    DCOPY(&n, V, &one, Z, &one);
  }
#else
  DCOPY(&n, vinit, &one, Z, &one);
#endif
  /*-------------------- normalize it */
  double t, nt, res0;
  if (ifGenEv) {
    /* B norm */
    matvec_B(Z, V);
    t = 1.0 / sqrt(DDOT(&n, V, &one, Z, &one));
    DSCAL(&n, &t, Z, &one);
  } else {
    /* 2-norm */
    t = 1.0 / DNRM2(&n, V, &one); // add a test here.
  }
  /* unit B^{-1}-norm or 2-norm */
  DSCAL(&n, &t, V, &one);
  /*-------------------- for ortho test */
  double wn = 0.0;
  int nwn = 0;
  /*-------------------- for stopping test [restricted trace]*/
  tr0 = 0;
  /*-------------------- lanczos vectors updated by rotating pointer*/
  /*-------------------- pointers to Lanczos vectors */
  double *zold, *z, *znew;
  double *v, *vnew;
  /*--------------------  Lanczos recurrence coefficients */
  double alpha, nalpha, beta=0.0, nbeta;
  int count = 0;
  // ---------------- main Lanczos loop 
  for (k=0; k<maxit; k++) {
    /*-------------------- quick reference to Z(:,k-1) when k>0 */
    zold = k > 0 ? Z+(k-1)*n_l : NULL;
    /*-------------------- a quick reference to V(:,k) */
    v = &V[k*n_l];
    /*-------------------- a quick reference to Z(:,k) */
    z = &Z[k*n_l];
    /*-------------------- next Lanczos vector V(:,k+1)*/
    vnew = v + n;
    /*-------------------- next Lanczos vector Z(:,k+1)*/
    znew = z + n;
    /*-------------------- compute w = rat * v */
    RatFiltApply(n, rat, v, znew, wk);
    /*------------------ znew = znew - beta*zold */
    if (zold) {
      nbeta = -beta;
      DAXPY(&n, &nbeta, zold, &one, znew, &one);
    }
    /*-------------------- alpha = znew'*v */
    alpha = DDOT(&n, v, &one, znew, &one);
    /*-------------------- T(k,k) = alpha */
    dT[k] = alpha;
    wn += fabs(alpha);
    /*-------------------- znew = znew - alpha*z */
    nalpha = -alpha;
    DAXPY(&n, &nalpha, z, &one, znew, &one);
    /*-------------------- FULL reortho to all previous Lan vectors */
    if (ifGenEv) {
      /* znew = znew - Z(:,1:k)*V(:,1:k)'*znew */
      CGS_DGKS2(n, k+1, NGS_MAX, Z, V, znew, wk);
      /* -------------- NOTE: B-matvec 
       *                vnew = B * znew */
      matvec_B(znew, vnew);
      /*-------------------- beta = (vnew, znew)^{1/2} */
      beta = sqrt(DDOT(&n, vnew, &one, znew, &one));
    } else {
      /* vnew = vnew - V(:,1:k)*V(:,1:k)'*vnew */
      /* beta = norm(vnew) */
      CGS_DGKS(n, k+1, NGS_MAX, V, vnew, &beta, wk);
    }
    wn += 2.0 * beta;
    nwn += 3;
    /*-------------------- lucky breakdown test */
    if (beta*nwn < orthTol*wn) {
      if (do_print) {
        fprintf(fstats, "it %4d: Lucky breakdown, beta = %.15e\n", k, beta);
      }
#if FILTER_VINIT
      /*------------------ generate a new init vector in znew */
      rand_double(n, vrand);
      /* Filter the initial vector */
      RatFiltApply(n, rat, vrand, znew, wk);
#else
      rand_double(n, znew);
#endif
      if (ifGenEv) {
	/* znew = znew - Z(:,1:k)*V(:,1:k)'*znew */
        CGS_DGKS2(n, k+1, NGS_MAX, Z, V, znew, wk);
	/* -------------- NOTE: B-matvec */
        matvec_B(znew, vnew);
        beta = sqrt(DDOT(&n, vnew, &one, znew, &one));
	/*-------------------- vnew = vnew / beta */
        t = 1.0 / beta;
        DSCAL(&n, &t, vnew, &one);
	/*-------------------- znew = znew / beta */
        DSCAL(&n, &t, znew, &one);
        beta = 0.0;
      } else {
	/* vnew = vnew - V(:,1:k)*V(:,1:k)'*vnew */
	/* beta = norm(vnew) */
        CGS_DGKS(n, k+1, NGS_MAX, V, vnew, &beta, wk);
	/*-------------------- vnew = vnew / beta */
        t = 1.0 / beta;
        DSCAL(&n, &t, vnew, &one);
        beta = 0.0;
      }
    } else {
      /*-------------------- vnew = vnew / beta */
      t = 1.0 / beta;
      DSCAL(&n, &t, vnew, &one);
      if (ifGenEv) {
        /*-------------------- znew = znew / beta */
        DSCAL(&n, &t, znew, &one);
      }
    }
    /*-------------------- T(k+1,k) = beta */
    eT[k] = beta;
#if 0
    /*-------------------- Reallocate memory if maxit is smaller than # of eigs */    
    if (k == maxit-1) {
      maxit = 1 + (int) (maxit * 1.5);
      Realloc(V, (maxit+1)*n_l, double);
      if (ifGenEv) {
	Realloc(Z, (maxit+1)*n_l, double);
      } else {
        /* make sure Z == V since V may be changed in the Realloc above */
        Z = V;
      }
      Realloc(dT,    maxit, double);
      Realloc(eT,    maxit, double);
      Realloc(Lam,   maxit, double);
      Realloc(res,   maxit, double);    
      Realloc(EvalT, maxit, double);
    }
#endif
    /*---------------------- test for Ritz vectors */
    if ( (k < Ntest || (k-Ntest) % cycle != 0) && k != maxit-1 ) {
      continue;
    }
    /*---------------------- diagonalize  T(1:k,1:k)       
                             vals in EvalT, vecs in EvecT  */
    kdim = k+1;
#if 1
    /*-------------------- THIS uses dsetv, do not need eig vector */    
    SymmTridEig(EvalT, NULL, kdim, dT, eT);
    count = kdim;
#else
    /*-------------------- THIS uses dstemr */
    double vl = bar - DBL_EPSILON, vu = 10000.0;  /* needed by SymmTridEigS */
    SymmTridEigS(EvalT, EvecT, kdim, vl, vu, &count, dT, eT);
#endif
    /*-------------------- restricted trace: used for convergence test */
    tr1 = 0;  
    /*-------------------- get residual norms and check acceptance of Ritz 
     *                     values for r(A). nconv records number of 
     *                     eigenvalues whose residual for r(A) is smaller 
     *                     than tol. */
    nconv = 0;
    for (i=0; i<count; i++) {
      flami = EvalT[i];
      if (flami + DBL_EPS_MULT * DBL_EPSILON >= bar) {
        tr1 += flami;
        nconv++;
      }
    }

    if (do_print) {
      fprintf(fstats, "k %4d: nconv %4d  tr1 %21.15e\n",
              k, nconv,tr1);
    }
    /* -------------------- simple test because all eigenvalues
                            are between gamB and ~1. */
    if ( (fabs(tr1-tr0) < 2e-12) || (fabs(tr1)+fabs(tr0)<1e-10) ) {
      break;
    }
    tr0 = tr1;
  } /* end of the main loop */

  if (k >= maxit) {
     fprintf(fstats, "The max number of iterations [%d] has been reached. The eigenvalues computed may not have converged\n", maxit);
  }

  /*-------------------- compute eig vals and vector */
  size_t kdim_l = kdim; /* just in case if kdim is > 65K */
  Malloc(EvecT, kdim_l*kdim_l, double); // Eigen vectors of T
  SymmTridEig(EvalT, EvecT, kdim, dT, eT);
  
  tt = evsl_timer();
  /*-------------------- done == compute Ritz vectors */
  Malloc(Rvec, nconv*n_l, double);       // holds computed Ritz vectors

  nev = 0;
  for (i=0; i<count; i++) {
    flami = EvalT[i];
    //-------------------- reject eigenvalue if rho(lam)<bar
    if (fabs(flami) < bar) {
      continue;
    }
    y = &EvecT[i*kdim_l];
    /*-------------------- make sure to normalize */
    /*
    t = DNRM2(&kdim, y, &one);
    t = 1.0 / t;
    DSCAL(&kdim, &t, y, &one);
    */
    /*-------------------- compute Ritz vectors 
     *                     NOTE: use Z for gen e.v */
    u = &Rvec[nev*n_l];
    DGEMV(&cN, &n, &kdim, &done, Z, &n, y, &one, &dzero, u, &one);
    /*-------------------- normalize u */
    if (ifGenEv) {
      /* B-norm, w2 = B*u */
      matvec_B(u, w2);
      t = sqrt(DDOT(&n, u, &one, w2, &one)); /* should be one */
    } else {
      /* 2-norm */
      t = DNRM2(&n, u, &one); /* should be one */
    }
    /*-------------------- return code 2 --> zero eigenvector found */
    if (t == 0.0) {
      return 2;
    }
    /*-------------------- scal u */
    t = 1.0 / t;
    DSCAL(&n, &t, u, &one);
    /*-------------------- scal B*u */
    if (ifGenEv) {
      /*------------------ w2 = B*u */
      DSCAL(&n, &t, w2, &one);
    }
    /*-------------------- w = A*u */
    matvec_A(u, wk);
    /*-------------------- Ritz val: t = (u'*w)/(u'*u)
                                     t = (u'*w)/(u'*B*u) */
    t = DDOT(&n, wk, &one, u, &one);
    /*-------------------- if lambda (==t) is in [a,b] */
    if (t < aa - DBL_EPS_MULT * DBL_EPSILON || t > bb + DBL_EPS_MULT * DBL_EPSILON) {
      continue;
    }
    /*-------------------- compute residual wrt A for this pair */
    nt = -t;
    if (ifGenEv) {
      /*-------------------- w = w - t*B*u */
      DAXPY(&n, &nt, w2, &one, wk, &one);
    } else {
      /*-------------------- w = w - t*u */
      DAXPY(&n, &nt, u, &one, wk, &one);
    }
    /*-------------------- if diag scaling is present */
    if (evsldata.ds) {
      /* scale the residual */
      int j;
      for (j=0; j<n; j++) {
        /* NOTE that we saved reciprocal of the diag scaling */
        wk[j] /= evsldata.ds[j];
      }
    }
    /*-------------------- res0 = 2-norm(wk) */
    res0 = DNRM2(&n, wk, &one); 
    /*--------------------   accept (t, y) */
    if (res0 < tol) {
      Lam[nev] = t;
      res[nev] = res0;
      nev++;
    }
  }
  tm1 = evsl_timer() - tt;
  
  /*-------------------- Done.  output : */
  *nevOut = nev;
  *lamo = Lam;
  *Wo = Rvec;
  *reso = res;
  /*-------------------- free arrays */
  free(V);
  free(dT);
  free(eT);
  free(EvalT);
  free(EvecT);
  free(wk);
  if (vrand) {
    free(vrand);
  }
  if (ifGenEv) {
    free(Z);
  }
  /*-------------------- record stats */
  tall = evsl_timer() - tall;

  evslstat.t_iter = tall;
  evslstat.t_ritz = tm1;

  return 0;
}

