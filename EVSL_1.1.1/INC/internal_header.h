/**
 * @file internal_header.h
 * @brief This file contains function prototypes and constant definitions
 * internally used in EVSL that should not be exposed to users.
 * This header file is supposed to be included inside SRC only
*/
#ifndef INTERNAL_HEADER_H
#define INTERNAL_HEADER_H

#include "evsl.h"

#include <stdlib.h>
#include <assert.h>
#include <math.h>

/*- - - - - - - - - blas and lapack */
#include "../SRC/blas/evsl_blas.h"
#include "../SRC/lapack/evsl_lapack.h"

/*- - - - - - - - - constants */
#define orthTol 1e-14

/*! max number of Gram-Schmidt process in orthogonalization */
#define NGS_MAX 2

/*- - - - - - - - - error handler */
#define CHKERR(ierr) assert(!(ierr))

/*- - - - - - - - - chebpoly.c */
int chebxCoefd(int m, double gam, int damping, double *mu);
int dampcf(int m, int damping, double *jac);
int chebxPltd(int m, double *mu, int n, double *xi, double *yi);
int ChebAv(polparams *pol, double *v, double *y, double *w);
void chext(polparams *pol, double aIn, double bIn);

/*- - - - - - - - - dos_utils.c */
int apfun(const double c, const double h, const double* xi, double (*ffun)(double), const int npts, double* yi);
double rec(const double a);
double isqrt(const double a);
int pnav(double *mu, const int m, const double cc, const double dd, double *v, double *y, double *w);
int lsPol(double (*ffun)(double), BSolDataPol *pol);

/*- - - - - - - - - dump.c */
void savemat(csrMat *A, const char *fn);
void savedensemat(double *A, int lda, int m, int n, const char *fn);
void save_vec(int n, const double *x, const char fn[]);

/*- - - - - - - - - misc_la.c */
int SymmTridEig(double *eigVal, double *eigVec, int n, const double *diag, const double *sdiag);
int SymmTridEigS(double *eigVal, double *eigVec, int n, double vl, double vu, int *nevO, const double *diag, const double *sdiag);
void SymEigenSolver(int n, double *A, int lda, double *Q, int ldq, double *lam);
void GenEigenSolver(int n, double *a, int lda, double *b, int ldb, double *v, int ldv, double *lam, double *work);
void CGS_DGKS(int n, int k, int i_max, double *Q, double *v, double *nrmv, double *w);
void CGS_DGKS2(int n, int k, int i_max, double *Z, double *Q, double *v, double *w);
void orth(double *V, int n, int k, double *Vo, double *work);
void orth2(double *a, int lda, int n, int k, double *work);

/*- - - - - - - - - ratfilter.c */
void contQuad(int method, int n, EVSL_Complex *zk);
void ratf2p2(int n, int *mulp, EVSL_Complex *zk, EVSL_Complex *alp, int m, double *z, double *x);
void pfe2(EVSL_Complex s1, EVSL_Complex s2, int k1, int k2, EVSL_Complex* alp, EVSL_Complex* bet);
EVSL_Complex integg2(EVSL_Complex s1, EVSL_Complex s2, EVSL_Complex* alp, int k1, EVSL_Complex* bet, int k2, double a, double b);
void weights(int n, EVSL_Complex* zk, int* pow, double lambda, EVSL_Complex* omega);
int scaleweigthts(int n, double a, double b, EVSL_Complex *zk, int* pow, EVSL_Complex* omegaM);

/*- - - - - - - - - ratlanNr.c */
void RatFiltApply(int n, int l, ratparams *rat, double *b, double *x, double *w3);

/*- - - - - - - - - - simpson.c */
void simpson(double *xi, double *yi, int npts);

/*- - - - - - - - - spmat.c */
// memory allocation/reallocation for a CSR matrix
void csr_resize(int nrow, int ncol, int nnz, csrMat *csr);
void sortrow(csrMat *A);

/*- - - - - - - - - timing.c */
int time_seeder();

/*- - - - - - - - - vect.c */
void vecset(int n, double t, double *v);
void vec_perm(int n, int *p, double *x, double *y);
void vec_iperm(int n, int *p, double *x, double *y);

/*------------------- inline functions */
/**
* @brief y = A * x
* This is the matvec function for the matrix A in evsldata
*/
static inline void matvec_A(double *x, double *y, int k) {
  CHKERR(!evsldata.Amv);
  double tms = evsl_timer();
  evsldata.Amv->func(x, y, k, evsldata.Amv->data);
  double tme = evsl_timer();
  evslstat.t_mvA += tme - tms;
  evslstat.n_mvA ++;
}

/**
* @brief y = B * x
* This is the matvec function for the matrix B in evsldata
*/
static inline void matvec_B(double *x, double *y, int k) {
  CHKERR(!evsldata.Bmv);
  double tms = evsl_timer();
  evsldata.Bmv->func(x, y, k, evsldata.Bmv->data);
  double tme = evsl_timer();
  evslstat.t_mvB += tme - tms;
  evslstat.n_mvB ++;
}

/**
* @brief y = B \ x
* This is the solve function for the matrix B in evsldata
*/
static inline void solve_B(double *x, double *y) {
  CHKERR(!evsldata.Bsol);
  double tms = evsl_timer();
  evsldata.Bsol->func(x, y, evsldata.Bsol->data);
  double tme = evsl_timer();
  evslstat.t_svB += tme - tms;
  evslstat.n_svB ++;
}

/**
* @brief y = LT \ x or y = SQRT(B) \ x
* This is the solve function for the matrix B in evsldata
*/
static inline void solve_LT(double *x, double *y) {
  CHKERR(!evsldata.LTsol);
  double tms = evsl_timer();
  evsldata.LTsol->func(x, y, evsldata.LTsol->data);
  double tme = evsl_timer();
  evslstat.t_svLT += tme - tms;
  evslstat.n_svLT ++;
}

/**
* @brief y = (A- sB) \ x
* This is the solve function for the complex shifted matrix
*/
static inline void solve_ASigB(EVSLASIGMABSol *sol, int n, int l,
                               double *br, double *bz,
                               double *xr, double *xz) {
  double tms = evsl_timer();
  (sol->func)(n, l, br, bz, xr, xz, sol->data);
  double tme = evsl_timer();
  evslstat.t_svASigB+= tme - tms;
  evslstat.n_svASigB ++;
}

/**
 * @brief check if an interval is valid
 * */
static inline int check_intv(double *intv, FILE *fstats) {
  /* intv[4]: ( intv[0], intv[1] ) is the inteval of interest
   *          ( intv[2], intv[3] ) is the spectrum bounds
   * return   0: ok
   *        < 0: interval is invalid
   */
  double a=intv[0], b=intv[1], lmin=intv[2], lmax=intv[3];
  if (a >= b) {
    if (fstats) {
      fprintf(fstats, " error: invalid interval (%e, %e)\n", a, b);
    }
    return -1;
  }

  if (a >= lmax || b <= lmin) {
    if (fstats) {
      fprintf(fstats, " error: interval (%e, %e) is outside (%e %e) \n",
              a, b, lmin, lmax);
    }
    return -2;
  }

  return 0;
}

#endif
