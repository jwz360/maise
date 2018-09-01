#include <string.h>
#include <omp.h>
#include "cdef.h"
#include "ndef.h"
#include "edef.h"
#include "cpot.h"
#include "nmlp.h"
#include "nmod.h"
#include "nutl.h"
#include "nmin.h"
#include "util.h"

#define x(X)  gsl_vector_get(x, X)
#define d(X)  gsl_vector_get(d, X)
#define p(X)  gsl_vector_get(p, X)
#define o(X)  gsl_vector_get(p, X)
#define w(X)  gsl_vector_get(w, X)
#define set(V, X, Y) gsl_vector_set(V, X, Y)

extern PAR *OOO;

long ECNT;
long FCNT;

int   CLL;

ANN  *RRR;
PRS  *PPP;
Cell *CCC;
LNK  *LLL;

void sig_term_handler(int signum, siginfo_t *info, void *ptr)
{
    printf("SIGTERM received.\n");
    if(RRR->MODT==1||RRR->MODT==13)
      SAVE_ANN(RRR,0.);
    if(RRR->MODT==3)
      SAVE_SC(RRR,0.);
}

void catch_sigterm()
{
    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &_sigact, NULL);
}
//================================================================
//  copy all MLP weights into a 1D vector of length R->NW
//================================================================
void W2X_GSL(gsl_vector *x)
{
  int i=0,k,n,m,spc;

  if(RRR->MODT==1||RRR->MODT==13)
  {
    if(RRR->MIX==0)
      for(spc=0;spc<RRR->NSPC;spc++)
      {
	// biases first
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    set(x,i++,RRR->B[spc][k][m]);
	
	// rest of the neurons next
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    for(n=0;n<RRR->NU[k];n++)
	      set(x,i++,RRR->W[spc][k][m][n]);
      }
    else
      for(spc=0,k=0;spc<RRR->NSPC;spc++)
        for(m=0;m<RRR->NU[k+1];m++)
          for(n=0;n<RRR->NU[k];n++)
            if(RRR->mask[spc][n]==1)
	      set(x,i++,RRR->W[spc][k][m][n]);
  }
  if(RRR->MODT==3)
    for(k=0;k<RRR->NW;k++)
      set(x,i++,RRR->WW[RRR->NSPC-1][k]);

}
//================================================================
//  copy weights from a 1D vector into the MLP's weights (W & B)
//================================================================
void X2W_GSL(const gsl_vector *x)
{
  int i=0,k,n,m,spc;

  if(RRR->MODT==1||RRR->MODT==13)
  {
    if(RRR->MIX==0)
      for(spc=0;spc<RRR->NSPC;spc++)
      {
	// biases first
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    RRR->B[spc][k][m] = x(i++);
	
	// rest of the neurons next
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    for(n=0;n<RRR->NU[k];n++)
	      RRR->W[spc][k][m][n] = x(i++);
      }
    else
      for(spc=0,k=0;spc<RRR->NSPC;spc++)
        for(m=0;m<RRR->NU[k+1];m++)
          for(n=0;n<RRR->NU[k];n++)
            if(RRR->mask[spc][n]==1)
              RRR->W[spc][k][m][n] = x(i++);
  }
  if(RRR->MODT==3)
    for(k=0;k<RRR->NW;k++)
      RRR->WW[RRR->NSPC-1][k] = x(i++);
}
//================================================================
double tot_err_gsl()
{
  int ii,n,q,N,m,k,spc,nth;
  double E;

  E = RRR->RE=RRR->RT=0.0;

  if(RRR->EFS==0)
  {
    omp_set_num_threads(RRR->NP);
    if(!SERIAL)
    #pragma omp parallel for reduction(+:E) schedule(dynamic,RRR->NB)
    for(n=0;n<RRR->N;n++)
      E += pow( (LLL[n].E-ENE_ANN(RRR,&LLL[n]))/(double)LLL[n].N,2.0 );
    RRR->RE = E;
  }

  if(RRR->EFS==1||RRR->EFS==3)
  {
    omp_set_num_threads(RRR->NP);
    if(!SERIAL)
    #pragma omp parallel private(nth)
    {
      nth = omp_get_thread_num();
      if(!SERIAL)
      #pragma omp for reduction(+:E) schedule(dynamic,RRR->NB)
//    for(nth=0;nth<RRR->NP;nth++)
      for(n=N=0;n<RRR->N;n++)
        E += pow( (LLL[n].E-FRC_ANN_PARA(RRR,&LLL[n],OOO[nth].e,OOO[nth].d))/(double)LLL[n].N,2.0 );
    }
    RRR->RE = E;
    
    for(n=N=0;n<RRR->N;n++)
    {
      for(ii=0;ii<LLL[n].NF;ii++) // consider only 'marked' atoms
	for(q=0;q<3;q++)
	  RRR->RT += pow( LLL[n].F[LLL[n].Fi[ii]][q] - LLL[n].f[LLL[n].Fi[ii]][q], 2.0);
      N += LLL[n].NF*3;
    }
  }

  RRR->RT = (RRR->RE*RRR->WE + RRR->RT*RRR->WF)/(double)RRR->N;

  if( RRR->MIX==0 )
    for(spc=0;spc<RRR->NSPC;spc++)
      {
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    RRR->RT += RRR->LREG*pow(RRR->B[spc][k][m],2.0);
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    for(n=0;n<RRR->NU[k];n++)
	      RRR->RT += RRR->LREG*pow(RRR->W[spc][k][m][n],2.0);
      }
  else
    for(spc=0;spc<RRR->NSPC;spc++)
      for(m=0,k=0;m<RRR->NU[k+1];m++)
        for(n=0;n<RRR->NU[k];n++)
          if(RRR->mask[spc][n]==1)
            RRR->RT += RRR->LREG*pow(RRR->W[spc][k][m][n],2.0);

  return RRR->RT;

}
//======================================================================
double func_gsl(const gsl_vector *x, void *params) 
{  
  double E;

  X2W_GSL(x);
  E = tot_err_gsl();  

  ECNT++;
  return E;
}
//======================================================================
// numerical derivatives
//======================================================================
void dfunc_num_gsl(gsl_vector *x, void *params, gsl_vector *d) 
{
  int i;
  double o,dx;
  int W;

  if(RRR->MIX==0) W=RRR->NW; else W=RRR->nw;
  dx = 0.000001;
  for(i=0;i<W;i++)
  {
    o    = x(i);
    set(x,i,o + dx);
    set(d,i,func_gsl(x,params));
    set(x,i,o - dx);
    set(d,i,(d(i)-func_gsl(x,params))/(2.0*dx));
    set(x,i,o);
  }  
  X2W_GSL(x);
  return;
}
//======================================================================
void dfunc_gsl(const gsl_vector *x, void* params, gsl_vector *d) 
{
  int i,k,n,m,spc;
  int nth;

  FCNT++;
  X2W_GSL(x);

  i=0;
  //===== for MLP =====
  if(RRR->MODT==1||RRR->MODT==13)
  {
    for(spc=0;spc<RRR->NSPC;spc++)
    {
      for(k=0;k<RRR->NL-1;k++)
	for(m=0;m<RRR->NU[k+1];m++)
	  RRR->Bp[spc][k][m] = 0.0;

      for(k=0;k<RRR->NL-1;k++)
	for(m=0;m<RRR->NU[k+1];m++)
	  for(n=0;n<RRR->NU[k];n++)
	    RRR->Wp[spc][k][m][n] = 0.0;
      
      for(nth=0;nth<RRR->NP;nth++)
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	  {
	    OOO[nth].Bp[spc][k][m] = 0.0;
	    for(n=0;n<RRR->NU[k];n++)
	      OOO[nth].Wp[spc][k][m][n] = 0.0;
	  }
    }

    omp_set_num_threads(RRR->NP);
    if(!SERIAL)
    #pragma omp parallel private(nth)
    {
      nth = omp_get_thread_num();
    if(!SERIAL)
      #pragma omp for schedule(dynamic,RRR->NB)
//    for(nth=0;nth<RRR->NP;nth++)
      for(n=0;n<RRR->N;n++)
	DE_ANN_PARA(RRR,&LLL[n],OOO[nth].Bp,OOO[nth].Wp,OOO[nth].e);
      if(RRR->EFS==1||RRR->EFS==3)
      {
    if(!SERIAL)
        #pragma omp for schedule(dynamic,RRR->NB)
// 	for(nth=0;nth<RRR->NP;nth++)
	for(n=0;n<RRR->N;n++)
	  DF_ANN_PARA(RRR,&LLL[n],OOO[nth].Bp,OOO[nth].Wp,OOO[nth].e,OOO[nth].d,OOO[nth].c,OOO[nth].xn,OOO[nth].xm);
      }
    }
    if(RRR->MODT==1||RRR->MODT==13)
      for(nth=0;nth<RRR->NP;nth++)
	for(spc=0;spc<RRR->NSPC;spc++)
	  for(k=0;k<RRR->NL-1;k++)
	    for(m=0;m<RRR->NU[k+1];m++)
	    {
	      RRR->Bp[spc][k][m] += OOO[nth].Bp[spc][k][m];
	      for(n=0;n<RRR->NU[k];n++)
		RRR->Wp[spc][k][m][n] += OOO[nth].Wp[spc][k][m][n];
	    }
 
    if(RRR->MIX==0)
      for(spc=0;spc<RRR->NSPC;spc++)
      {
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    set(d,i++,RRR->Bp[spc][k][m]/(double)RRR->N + 2.0*RRR->LREG*RRR->B[spc][k][m]);
	for(k=0;k<RRR->NL-1;k++)
	  for(m=0;m<RRR->NU[k+1];m++)
	    for(n=0;n<RRR->NU[k];n++)
	      set(d,i++,RRR->Wp[spc][k][m][n]/(double)RRR->N + 2.0*RRR->LREG*RRR->W[spc][k][m][n]);  
      }
    else
      for(spc=0;spc<RRR->NSPC;spc++)
        for(m=0,k=0;m<RRR->NU[k+1];m++)
          for(n=0;n<RRR->NU[k];n++)
            if(RRR->mask[spc][n]==1)
              set(d,i++,RRR->Wp[spc][k][m][n]/(double)RRR->N + 2.0*RRR->LREG*RRR->W[spc][k][m][n]);
  }
  //===== for SC  =====
  if(RRR->MODT==3)
  {
    for(k=0;k<RRR->NW;k++)
      RRR->WWp[RRR->NSPC-1][k] = 0.0;
    for(n=0;n<RRR->N;n++)
      DE_SC_(RRR,&LLL[n]);
    for(k=0;k<RRR->NW;k++)
      set(d,i++,RRR->WWp[RRR->NSPC-1][k]/(double)RRR->N);
  }

  return;
}
//======================================================================
// Compute both f and df together.
//======================================================================
void fdfunc_gsl(const gsl_vector *x, void *params, double *f, gsl_vector *df) 
{
  *f = func_gsl(x, params); 
  dfunc_gsl(x, params, df);
}
//======================================================================
double bfgs_mlp(ANN *R, LNK *L)
{
  double *V,E,O;
  int j,N,status;
  gsl_vector *p, *o;
  void *params;
  char add_m[200],min[200];
  FILE *out;

  RRR = R; 
  LLL = L;

  params = NULL;
  ECNT = 0;

  sprintf(add_m,"%s/out.dat",RRR->otpt);

  if(RRR->MIX==0)  N = RRR->NW; 
  else             N = RRR->nw;

  if(RRR->MINT==0)
    sprintf(min,"%s","BFGS2");
  if(RRR->MINT==1)
    sprintf(min,"%s","CG-PR");
  if(RRR->MINT==2)
    sprintf(min,"%s","CG-FR");
  if(RRR->MINT==3)
    sprintf(min,"%s","STPDT");

  printf("%s relaxation: %d DOF\n",min,N);

  out = fopen(add_m,"a");
  fprintf(out,"%s relaxation: %d DOF\n",min,N);
  fclose(out);

  p = gsl_vector_alloc (N);
  o = gsl_vector_alloc (N);
  V = make_d1D(RRR->NW);
  
  W2X_GSL(p);

  for(j=0;j<N;j++)
    set(o,j,p(j));

  RRR->ITER = 0;

  const gsl_multimin_fdfminimizer_type *T;
  gsl_multimin_fdfminimizer *s;

  gsl_multimin_function_fdf gsl_fdf;

  gsl_fdf.n = N;
  gsl_fdf.f = func_gsl;
  gsl_fdf.df = dfunc_gsl;
  gsl_fdf.fdf = fdfunc_gsl;
  gsl_fdf.params = params;

  O = func_gsl(p, params);
  dfunc_gsl(p,params,o);
  catch_sigterm();


  T = gsl_multimin_fdfminimizer_vector_bfgs2;
  // inconsistent with c-bfgs
  if(RRR->MINT==0)
    T = gsl_multimin_fdfminimizer_vector_bfgs2;
  if(RRR->MINT==1)
    T = gsl_multimin_fdfminimizer_conjugate_pr;
  if(RRR->MINT==2)
    T = gsl_multimin_fdfminimizer_conjugate_fr;
  if(RRR->MINT==3)
    T = gsl_multimin_fdfminimizer_steepest_descent;

  s = gsl_multimin_fdfminimizer_alloc (T, N);

  double tol=1e-5;
  gsl_multimin_fdfminimizer_set (s, &gsl_fdf, p, 0.01, tol);
  int old_iter = 0;
    
  do {
    RRR->ITER++;
    status = gsl_multimin_fdfminimizer_iterate (s);
    if (status) {
      tol*=10;
      gsl_multimin_fdfminimizer_set (s, &gsl_fdf, s->x, 0.01, tol);
      gsl_multimin_fdfminimizer_restart(s);
      old_iter = RRR->ITER;
    } else if (tol< 2. && RRR->ITER- old_iter > 20) {
      tol/=10;
      gsl_multimin_fdfminimizer_set (s, &gsl_fdf, s->x, 0.01, tol);
      gsl_multimin_fdfminimizer_restart(s);
      old_iter = RRR->ITER;
    }

    R->EE= 0.0;
    SAVE_ANN(RRR,0.0);
    status = gsl_multimin_test_gradient (s->gradient, 2.27e-19);
    printf("%6d %18.16lf\n",RRR->ITER, sqrt(tot_err_gsl()));
    out = fopen(add_m,"a");
    fprintf(out,"%6d %18.16lf\n",RRR->ITER, sqrt(tot_err_gsl()));
    fclose(out);

  } while (status == GSL_CONTINUE && RRR->ITER < RRR->MITR);
  
  if(RRR->MODT==1||RRR->MODT==13)
    SAVE_ANN(RRR,0.);
  if(RRR->MODT==3)
    SAVE_SC(RRR,0.);

  E = func_gsl(s->x, params);
  printf("% lf\n",E);

  X2W_GSL(s->x);

  printf("RES = % lf % lf % lf\n",O,E,E-O);

  gsl_vector_free (p);
  gsl_vector_free (o);
  free_d1D(V);
  gsl_multimin_fdfminimizer_free (s);

  printf("ERROR ECNT CALLS = %6ld\n",ECNT);
  printf("ERROR FCNT CALLS = %6ld\n",FCNT);

  out = fopen(add_m,"a");
  fprintf(out,"ERROR ECNT CALLS = %6ld\n",ECNT);
  fprintf(out,"ERROR FCNT CALLS = %6ld\n",FCNT);
  fprintf(out,"RES = % lf % lf % lf\n",O,E,E-O);
  fclose(out);

  return 0.0;
}