/*
 * qam_gmi_mex.c - Compute MI and GMI for QAM
 *
 * Use this function to compute MI and GMI for M-QAM
 * constellations.
 *
 * Compile with: mex -lm -R2018a CFLAGS="$CFLAGS -fopenmp" LDFLAGS="$LDFLAGS -fopenmp" qam_gmi_mex.c
 * (requires MATLAB R2018a or newer versions)
 *
 * 2018 - Dario Pilori <dario.pilori@polito.it>
 */
#include <math.h>
#include <complex.h>
#include <omp.h>
#include "mex.h"
#define N_GH 10

/* Gauss-Hermite parameters */
const double x[N_GH] = {-3.436159118837737603327,
-2.532731674232789796409,
-1.756683649299881773451,
-1.036610829789513654178,
-0.3429013272237046087892,
0.3429013272237046087892,
1.036610829789513654178,
1.756683649299881773451,
2.532731674232789796409,
3.436159118837737603327};
const double w[N_GH] = {7.64043285523262062916E-6,
0.001343645746781232692202,
0.0338743944554810631362,
0.2401386110823146864165,
0.6108626337353257987836,
0.6108626337353257987836,
0.2401386110823146864165,
0.03387439445548106313617,
0.001343645746781232692202,
7.64043285523262062916E-6};

double symbol_energy(const double complex *C, int M);
unsigned int insert_zero(unsigned int i, unsigned int k, unsigned int nb);
double qam_eval_mi(const double complex *C, int M, double s);
double qam_eval_gmi(const double complex *C, int M, double s);

/* Gateway function */
void mexFunction(int nlhs, mxArray *plhs[],
        int nrhs, const mxArray *prhs[])
{
    size_t M, N;                   /* constellation and SNR size */
    int i;                        /* loop index */
    double complex *C;
    double *mi, *gmi, *snr;
    double Es, sigma;
    
    /* Verify input */
    if(nrhs != 2) {
        mexErrMsgIdAndTxt("dsp-library:qam_gmi_mex:nrhs",
                "Two inputs required.");
    }
    
    /* Verify output */
    if(nlhs > 2) {
        mexErrMsgIdAndTxt("dsp-library:qam_gmi_mex:nlhs",
                "Max two outputs.");
    }
    
    /* Get sizes */
    M = mxGetM(prhs[0]);
    N = mxGetM(prhs[1]);
        
    /* Get pointers */
    snr = mxGetPr(prhs[1]);
    C = (double complex *) mxGetComplexDoubles(prhs[0]);
    
    /* create the output matrices */
    plhs[0] = mxCreateDoubleMatrix((mwSize)N,1,mxREAL);
    plhs[1] = mxCreateDoubleMatrix((mwSize)N,1,mxREAL);
    
    /* get a pointer to the real data in the output matrix */
    mi = mxGetDoubles(plhs[0]);
    gmi = mxGetDoubles(plhs[1]);
    
    /* Calculate symbol energy */
    Es = symbol_energy(C, M);
    
    /* Call functions */
    #pragma omp parallel for private(sigma)
    for(i=0; i<N; i++) {
        sigma = sqrt(Es) * pow(10,-snr[i]/20);
        mi[i] = qam_eval_mi(C, M, sigma);
        gmi[i] = qam_eval_gmi(C, M, sigma);
    }
}

/* Helper function to calculate symbol energy */
double symbol_energy(const double complex *C, int M)
{
    int i;
    double Es = 0.0;
    
    for(i=0;i<M;i++)
    {
        Es += pow(cabs(C[i]),2.0);
    }
    Es /= M;
    
    return Es;
}

/* Helper function to insert a zero inside a number */
unsigned int insert_zero(unsigned int i, unsigned int k, unsigned int nb)
{
    unsigned int b0, left, right;
    
    left = (i<<1) & (  ( (1<<(nb-k)) - 1)<<(k+1) );
    right = i & ((1<<k)-1);
    b0 = left | right;
    
    return b0;
}


/* Calculate AWGN mutual information for PAM */
double qam_eval_mi(const double complex *C, int M, double s)
{
    double MI = 0;
    double tmp;
    int i, l1, l2, j;
    
    /* Cycle through constellation point */
    for(i=0; i<M; i++)
    {
        /* Cycle through Gauss-Hermite parameters */
        for(l1=0; l1<N_GH; l1++)
        {
            for(l2=0; l2<N_GH; l2++)
            {
                tmp = 0.0;
                
                /* Cycle through constellation point for the logarithm */
                for(j=0; j<M; j++)
                {
                    tmp += exp(-( pow(cabs(C[j]-C[i]),2.0) -
                            2*s*creal((x[l1]+I*x[l2])*(C[j]-C[i])) )/pow(s,2.0));
                }
                
                MI -= w[l1]*w[l2]*log2(tmp);
            }
        }
    }
    
    /* Prepare output */
    MI /= M*M_PI;
    MI += log2(M);
    
    return MI;
}

/* Calculate BICM mutual information (GMI) for equiprobable PAM */
double qam_eval_gmi(const double complex *C, int M, double s)
{
    const int m = log2(M);
    
    int i, l1, l2, j, k, b;
    int bi, bj;
    double GMI = 0;
    double tmp_num, tmp_den;
    
    /* Cycle through constellation bit */
    for(k=0; k<m; k++)
    {
        /* Bit can be either 0 or 1 */
        for(b=0; b<=1; b++)
        {
            /* Constellation points where k-th bit is equal to b */
            for(i=0; i<M/2; i++)
            {
                bi = insert_zero(i, k, m) + (b<<k);
                
                /* Cycle through Gauss-Hermite parameters */
                for(l1=0; l1<N_GH; l1++)
                {
                    for(l2=0; l2<N_GH; l2++)
                    {
                        /* Initialize numerator and denominator of the logarithm */
                        tmp_num = 0.0;
                        tmp_den = 0.0;
                        
                        /* Numerator of the logarithm */
                        for(j=0; j<M; j++)
                        {
                            tmp_num += exp(-(pow(cabs(C[bi]-C[j]),2.0) -
                                    2*s*creal((x[l1]+I*x[l2])*(C[bi]-C[j])))/pow(s,2.0));
                        }
                        
                        /* Denominator of the logarithm */
                        for(j=0; j<M/2; j++)
                        {
                            bj = insert_zero(j, k, m) + (b<<k);
                            tmp_den += exp(-(pow(cabs(C[bi]-C[bj]),2.0) -
                                    2*s*creal((x[l1]+I*x[l2])*(C[bi]-C[bj])))/pow(s,2.0));
                        }
                        
                        /* Evaluate GMI */
                        GMI -= w[l1]*w[l2]*log2(tmp_num/tmp_den);
                    }
                }
            }
        }
    }
    
    /* Prepare output */
    GMI /= M*M_PI;
    GMI += m;
    
    return GMI;
}