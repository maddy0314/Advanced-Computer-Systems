/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#include <stdio.h>     // printf
#include <stdlib.h>    // free
#include <string.h>    // memset, strcat, strlen
#include <zstd.h>      // presumes zstd library is installed
#include <pthread.h>
#include "common.h"    // Helper functions, CHECK(), and CHECK_ZSTD()

//For passing data into pthreads
typedef struct pthreadWrapper {
    int id;
    ZSTD_CCtx* context;
    char* inPtr; //Read pointer in input buffer
    size_t inSize;
    char* outPtr; //Write pointer in output buffer
    size_t outSize;
    size_t outPos;
    int cLevel; // Compression level
} pthreadWrapper_t;

static void *pthreadCompressor(void* args) {
    printf("entered compressor\n");
    struct pthreadWrapper* ptw = (struct pthreadWrapper*)args;
    printf("Hello World! It's me, thread #%d!\n", ptw->id);

    /* Create the context. */
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    CHECK(cctx != NULL, "ZSTD_createCCtx() failed!");

    /* Set any parameters you want.
     * Here we set the compression level, and enable the checksum.
     */
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, ptw->cLevel) );
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1) );
    //ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, nbThreads);

    ZSTD_inBuffer input = { ptw->inPtr, ptw->inSize, 0 };

    ptw->outPtr = malloc_orDie(ptw->inSize);
    ZSTD_outBuffer output = { ptw->outPtr, ptw->outSize, 0 };

    size_t const remaining = ZSTD_compressStream2(cctx, &output , &input, ptw->cLevel);
    //CHECK_ZSTD(remaining);

    ptw->outPos = output.pos;

    ZSTD_freeCCtx(cctx);
    //free(ptw->outPtr);

    return NULL;

    //pthread_exit(NULL);
}

static char* createOutFilename_orDie(const char* filename) {
    size_t const inL = strlen(filename);
    size_t const outL = inL + 5;
    void* const outSpace = malloc_orDie(outL);
    memset(outSpace, 0, outL);
    strcat(outSpace, filename);
    strcat(outSpace, ".zst");
    return (char*)outSpace;
}

int main(int argc, const char** argv) {
    const char* const exeName = argv[0];

    if (argc < 2) {
        printf("wrong arguments\n");
        printf("usage:\n");
        printf("%s FILE [LEVEL] [THREADS]\n", exeName);
        return 1;
    }

    int cLevel = 1;
    int nbThreads = 4;

    if (argc >= 3) {
      cLevel = atoi (argv[2]);
      CHECK(cLevel != 0, "can't parse LEVEL!");
    }

    if (argc >= 4) {
      nbThreads = atoi (argv[3]);
      CHECK(nbThreads != 0, "can't parse THREADS!");
    }

    const char* const inFilename = argv[1];

    char* const outFilename = createOutFilename_orDie(inFilename);

//MAIN THREAD: PREP THREADS
    pthread_t pthreads[nbThreads];
    struct pthreadWrapper wrappers[nbThreads];

//MAIN THREAD: INITIALIZE FILES BUFFERS & STREAMS
    /* Open the input and output files. */
    FILE* const fin  = fopen_orDie(inFilename, "rb");
    FILE* const fout = fopen_orDie(outFilename, "wb");
    /* Create the input and output buffers.
     * They may be any size, but we recommend using these functions to size them.
     * Performance will only suffer significantly for very tiny buffers.
     */
    //size_t const buffInSize = ZSTD_CStreamInSize();
    //void* buffIn;
    //size_t const buffOutSize = ZSTD_CStreamOutSize();
    //void*  const buffOut = malloc_orDie(buffOutSize);



    

//LOOP MAIN THREAD: READ A CHUNK OF THE FILE
    size_t const toRead = 16*1024;
    int lastChunk = 0;
    char* running = malloc(sizeof(char)*nbThreads);
    memset(running, 0, nbThreads);

    for(int i = 0; i <nbThreads; i++) {
        printf("%c\n", running[i]);
    }
    printf("\n");

    char* testptr;

    for (;;) {
        for(int i = 0; i < nbThreads; i++) {
            struct pthreadWrapper ptw;
            ptw.inPtr = malloc_orDie(toRead);
            size_t read = fread_orDie(ptw.inPtr, toRead, fin);

            /* Select the flush mode.
             * If the read may not be finished (read == toRead) we use
             * ZSTD_e_continue. If this is the last chunk, we use ZSTD_e_end.
             * Zstd optimizes the case where the first flush mode is ZSTD_e_end,
             * since it knows it is compressing the entire source in one pass.
             */
            lastChunk = (read < toRead);
            ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;

    //LOOP MAIN THREAD: MAKE THREAD FOR CURRENT CHUNK

            if(read > 0) {
                ptw.id = i;
                //ptw.context = cctx;
                ptw.inSize = read;
                ptw.cLevel = cLevel;

                running[i] = 1;

                wrappers[i] = ptw;
                pthread_create(pthreads+i, NULL, pthreadCompressor, (void*)(&wrappers[i]));
                
            }
        }

        for(int i = 0; i < nbThreads; i++) {
            if(running[i] == 1) {
                pthread_join(pthreads[i],NULL);
                fwrite_orDie(wrappers[i].outPtr, wrappers[i].outPos, fout);
            }
            running[i] = 0;
        }

        if (lastChunk) {
            break;
        }
    }

    fclose_orDie(fin);
    fclose_orDie(fout);

    for(int i = 0; i < nbThreads; i++) {
        free(wrappers[i].inPtr);
        free(wrappers[i].outPtr);
    }
}