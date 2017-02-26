//Name: Mimi Chen AndrewId: mimic1
/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, ii, jj;
    int tmp0, tmp1, tmp2, tmp3; 
    int tmp4, tmp5, tmp6, tmp7;
    
    //Handle the matric 32*32
    if((M == 32) && (N == 32))
    {
        for (ii = 0; ii < N; ii +=8)
        {
            for(jj = 0; jj < M; jj += 8 )
            {
                for(i = ii; i < ii + 8; i++)
                {
                    for(j = jj; j < jj + 8; j ++)
                    {
                        if(i != j)
                        {
                            tmp1 = A[i][j];
                            B[j][i] = tmp1;  
                        }
                        else
                        {
                            tmp2 = i;
                            tmp3 = A[i][j];
                        }
                    }
                    // As what we could see, 
                    // if we did not store the diagonal one;
                    // it will re-access it, store and load,
                    // which will cause extra miss.
                    if(ii == jj){
                        B[tmp2][tmp2]= tmp3;
                    }
                }
            }
        }
    }
    
    //Handle the matric 64*64
    if((M == 64) && (N == 64))
    {
        //First , we divide it into 8*8 matrixs
       for (i = 0; i < N; i += 8)
       {
           for (j = 0; j < M; j += 8)
           {
               
	           tmp0 = A[j][i + 4];
               tmp1 = A[j][i + 5];
               tmp2 = A[j][i + 6];
               tmp3 = A[j][i + 7];
               
               //Then , we transpose the 4*8 matrix
               // which is now in the cache first;
               for(ii = 0; ii < 8; ii++)
               {
                   tmp4 = A[j + ii][i];
                   tmp5 = A[j + ii][i + 1];
                   tmp6 = A[j + ii][i + 2];
                   tmp7 = A[j + ii][i + 3];

                   B[i][j+ ii] = tmp4;
                   B[i][j + ii + 64] = tmp5;
                   B[i][j + ii + 128] = tmp6;
                   B[i][j + ii + 192] = tmp7;
		
               }
               
               //Then , we transpose the 4*8 matrix
               // which is is not int the cache.
               for(ii = 7; ii > 0; ii--)
               {
                   tmp4 = A[j + ii][i + 4];
                   tmp5 = A[j + ii][i + 5];
                   tmp6 = A[j + ii][i + 6];
                   tmp7 = A[j + ii][i + 7];

                   B[i + 4][j+ ii] = tmp4;
                   B[i + 4][j + ii + 64] = tmp5;
                   B[i + 4][j + ii + 128] = tmp6;
                   B[i + 4][j + ii + 192] = tmp7;
   		
               }
               // if I did not give number to B first
               // then it will miss the first line
	           B[i + 4][j + ii] = tmp0; //A[j][i + 4];
	           B[i + 5][j + ii] = tmp1; //A[j][i + 5];
	           B[i + 6][j + ii] = tmp2; //A[j][i + 6];
               B[i + 7][j + ii] = tmp3; //A[j][i + 7];
    
           }
       }
    }
    
   
    //Handle the matric 61*67
    if(M == 61 && N == 67)
    {
        for (ii = 0; ii < 67; ii += 16)
        {
            for(jj = 0; jj < 61; jj += 16 )
            {
                for(i = ii; (i < ii + 16) && (i < 67); i++)
                {
                    for(j = jj; (j < jj + 16) && ( j < 61); j++)
                    {
                        if(i != j)
                        {
                            tmp1 = A[i][j];
                            B[j][i] = tmp1;  
                        }
                        else
                        {
                            tmp2 = i;
                            tmp3 = A[i][j];
                        }
                    }
                    // As what we could see,
                    // if we did not store the diagonal one;
                    // it will re-access it, store and load,
                    // which will cause extra miss.
                    if(ii == jj){
                        B[tmp2][tmp2]= tmp3;
                    }
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

