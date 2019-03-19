////////////////////////////////////////////////////////////////////////////////
// Main File:       csim.c
// This File:       csim.c
//
// Author:          Natalie Brooks
// Email:           nrbrooks@wisc.edu
//
////////////////////////////////////////////////////////////////////////////////

/* Name: Natalie Brooks
 * CS login: natalie
 * Section(s): Section 001
 *
 * csim.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss plus a possible eviction.
 *  2. Instruction loads (I) are ignored.
 *  3. Data modify (M) is treated as a load followed by a store to the same
 *  address.
 *
 * The function printSummary() prints the number of hits, misses and evictions.
 */

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/* Globals set by command line args */
int s = 0; /* set index bits */
int E = 0; /* associativity */
int b = 0; /* block offset bits */
int verbosity = 0; /* print trace if set */
char* trace_file = NULL;

/* Derived from command line args */
int B; /* block size (bytes) B = 2^b */
int S; /* number of sets S = 2^s */

/* Counters used to record cache statistics */
int hit_cnt = 0;
int miss_cnt = 0;
int evict_cnt = 0;

/* Type: Memory address 
 * Type used for addresses or address masks
 */
typedef unsigned long long int mem_addr_t;

/* Type: Cache line */
typedef struct cache_line {                     
    char valid;
    mem_addr_t tag;
    int counter;
} cache_line_t;

typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;


/* The cache we are simulating */
cache_t cache;  

/* initCache - 
 * Allocate data structures to hold info regrading the sets and cache lines
 */
void initCache() {
    S = pow(2, s);
    cache = malloc(S * sizeof(cache_set_t));
    if ( cache == NULL ) {
        printf("Cannot malloc for cache.");
        exit(1);
    }
    
    for ( int i = 0; i < S; i++ ) {
        *(cache + i) = malloc(E * sizeof(cache_line_t));
        if ( *(cache + i) == NULL ) {
            printf("Cannot malloc for cache set.");
            // demalloc for earlier mallocs
            exit(1);
        }
        for ( int j = 0; j < E; j++ ) {
            cache_line_t *curLine = (cache_line_t*)(*(cache + i) + j);
            curLine->valid = '0';
            curLine->tag = 0;
            curLine->counter = 0;
        }
    } 
}


/* freeCache - free each piece of memory allocated using malloc 
 * inside initCache() function
 */
void freeCache() {
    for ( int i = 0; i < S; i++ ) {
        free(*(cache + i));
    }
    free(cache);              
}

/* accessData - Access data at memory address addr.
 *   If it is already in cache, increase hit_cnt
 *   If it is not in cache, bring it in cache, increase miss count.
 *   Also increase evict_cnt if a line is evicted.
 */
void accessData(mem_addr_t addr) { 
    S = pow(2, s);
    B = pow(2, b);
    // calculate set
    mem_addr_t set = (addr / B) % S;
    // calculate tag
    mem_addr_t tag = addr / (B * S); 
    
    int hit = 0;
    int located = 0;
    cache_line_t *curLine = NULL;
    cache_line_t *counterLine = NULL;
    int prevCounter;
    for ( int i = 0; i < E; i++ ) {
        curLine = (cache_line_t*)(*(cache + set) + i);
        // current line is valid and matches tag
        if ( (curLine->valid == '1') && (curLine->tag == tag) ) {
            hit_cnt++;
            prevCounter = curLine->counter;
            // set line's counter to most recently used
            curLine->counter = E + 1;
            hit = 1;
            break;
        }
    }

    if ( !hit ) {
        miss_cnt++;
        for ( int j = 0; j < E; j++ ) {
            curLine = (cache_line_t*)(*(cache + set) + j);
            // current line is empty and can be filled
            if ( curLine->valid == '0' ) {
                curLine->tag = tag;
                curLine->valid = '1';
                prevCounter = curLine->counter;
                // set line's counter to most recently used
                curLine->counter = E + 1;
                located = 1;
                break;
            }
        }
        
        // if tag is still unlocated, must evict LRU line
        if ( !located ) {
            evict_cnt++;
            for ( int k = 0; k < E; k++ ) {
                curLine = (cache_line_t*)(*(cache + set) + k);
                // LRU line will have counter val of 1
                if ( curLine->counter == 1 ) {
                    curLine->tag = tag;
                    prevCounter = curLine->counter;
                    // set line's counter to most recently used
                    curLine->counter = E + 1;
                    break;
                }
            }
        }
    }

    // deincrement counter of all lines above old line's counter value
    for ( int l = 0; l < E; l++ ) {
        counterLine = (cache_line_t*)(*(cache + set) + l);
        if ( counterLine->counter > prevCounter ) {
            counterLine->counter--;
        }   
    }  
}

/* replayTrace - replays the given trace file against the cache 
 * reads the input trace file line by line
 * extracts the type of each memory access : L/S/M
 * TRANSLATE one "L" as a load i.e. 1 memory access
 * TRANSLATE one "S" as a store i.e. 1 memory access
 * TRANSLATE one "M" as a load followed by a store i.e. 2 memory accesses 
 */
void replayTrace(char* trace_fn) {                      
    char buf[1000];
    mem_addr_t addr = 0;
    unsigned int len = 0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if (!trace_fp) {
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while (fgets(buf, 1000, trace_fp) != NULL) {
        if (buf[1] == 'S' || buf[1] == 'L' || buf[1] == 'M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);
      
            if (verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);
            
            if ( buf[1] == 'S' || buf[1] == 'L' ) {
                accessData(addr);
            }
            if ( buf[1] == 'M' ) {
                accessData(addr);
                accessData(addr);
            } 

            if (verbosity)
                printf("\n");
        }
    }

    fclose(trace_fp);
}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[]) {                 
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

/*
 * printSummary - Summarize the cache simulation statistics.
 */
void printSummary(int hits, int misses, int evictions) {                        
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}

/*
 * main - Main routine 
 */
int main(int argc, char* argv[]) {                      
    char c;
    
    // Parse the command line arguments: -h, -v, -s, -E, -b, -t 
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (c) {
            case 'b':
                b = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'h':
                printUsage(argv);
                exit(0);
            case 's':
                s = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'v':
                verbosity = 1;
                break;
            default:
                printUsage(argv);
                exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }

    /* Initialize cache */
    initCache();

    replayTrace(trace_file);

    /* Free allocated memory */
    freeCache();

    /* Output the hit and miss statistics */
    printSummary(hit_cnt, miss_cnt, evict_cnt);
    return 0;
}
