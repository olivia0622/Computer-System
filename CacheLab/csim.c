//  Name : Mimi Chen AndrewId = mimic1
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "cachelab.h"

//Define a struct for info of each cache line
typedef struct
{
    
    int valid;
    int tag;
    int time_stamp;
    
}cache_line;

int main (int argc, char **argv)
{
    
    int hit_count = 0;
    int miss_count = 0;
    int eviction_count = 0;
    int time_stamp = 0;
    int verbose_flag = 0;
    int set_index = 0;
    int associativity_lines = 0;
    int block_bits = 0;
    char *trace_file = NULL;
    int c;
    
    // Read the command line for the configure parameters
    while ((c = getopt (argc, argv, "vs:E:b:t:")) != -1)
    {
        switch (c)
        {
            case 'v':
              verbose_flag = 1;
              break;
            case 's':
              set_index = atoi(optarg);
              break;
            case 'E':
              associativity_lines = atoi(optarg);
              break;
            case 'b':
              block_bits = atoi(optarg);
              break;
            case 't':
              trace_file = optarg;
              break;
            default:
              printf("Wrong argument\n");
              break;
        }
    }
    
    //dynamically allocate the 2-dimensional array for
    //info of each cache_line
    long S = 1 << set_index;
    int E = associativity_lines;
    cache_line **cache = (cache_line **)malloc(S * sizeof(cache_line *));
    for(long i = 0; i < S; ++i)
      cache[i] = (cache_line *)malloc(E * sizeof(cache_line));
    
    //Initialize the array   
    for(long i = 0; i < S; ++i)
    {
        for(int j = 0; j < E; ++j )
        {
            cache[i][j].valid=0;
            cache[i][j].tag=0;
            cache[i][j].time_stamp=0;
        }
    }
    
    // open the trace file and the the memory trace
    char type;
    unsigned long addr = 0;
    int size;
    FILE * pFile;
    pFile = fopen(trace_file,"r");
    while(fscanf(pFile," %c %lx,%d", &type, &addr,&size)>0)
    {
        int empty_line = 0;
        int full_valid_flag = 1;
        int miss_flag = 1;
        unsigned temp = ~0;
        unsigned temp_block = ~(temp << set_index);
    
        //Get tag and set 
        long tag = addr >> (set_index + block_bits);
        int set = (addr >> block_bits) & temp_block;
        
        //Check whether this command line will hit
        for(int i = 0; i < E; ++i)
        {
            if(cache[set][i].valid == 1 )
            {
                //The condition that hit.
                if(cache[set][i].tag == tag)
                {
                    hit_count++;
                    time_stamp++;
                    cache[set][i].time_stamp = time_stamp;
                    miss_flag = 0;
                    if(verbose_flag == 1)
                    {
                        printf("%c %lx,%d hit\n", type, addr, size);
                    }
                }
            }
            // to check whether it is all valid lines in this set
            else if(cache[set][i].valid == 0)
            {
                full_valid_flag = 0;
                empty_line = i;
            }
        }
    
        // From the above forloop if we can not find one which let it hit
        // then it miss;
        if(miss_flag == 1 && full_valid_flag == 0)
        {
            
            miss_count = miss_count + 1;
            time_stamp++;
            cache[set][empty_line].time_stamp = time_stamp;
            cache[set][empty_line].valid = 1;
            cache[set][empty_line].tag = tag;
            
            if(verbose_flag == 1)
            {
                printf("%c %lx,%d miss\n", type, addr, size);
            }
        }
        
        //The condition that it miss eviction
        if(miss_flag == 1 && full_valid_flag == 1)
        { 
            miss_count = miss_count + 1;
            eviction_count = eviction_count + 1;
            time_stamp++;
            int temp_index = 0;
            int temp_time = time_stamp;
            
            //Get the Latest Recently time_stamp
            for(int i = 0; i <  E; ++i)
            {
                if(temp_time > cache[set][i].time_stamp)
                {
                    temp_index = i;
                    temp_time = cache[set][i].time_stamp;
                    
                }
            }
            cache[set][temp_index].tag = tag;
            cache[set][temp_index].time_stamp = time_stamp;
            cache[set][temp_index].valid = 1;
            
            if(verbose_flag == 1)
            {
                printf("%c %lx,%d miss eviction\n", type, addr, size);
            }
        }
    }
    fclose(pFile);
    printSummary(hit_count, miss_count, eviction_count);
    
    //free malloc;
    for(long i = 0; i < S; ++i)
    {
        free(cache[i]);
    }
    free(cache);
    return 0;
}

