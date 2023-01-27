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

#if 0
static void compressFile_orDie(const char* fname, const char* outName, int cLevel,
                               int nbThreads)
{
    fprintf (stderr, "Starting compression of %s with level %d, using %d threads\n",
             fname, cLevel, nbThreads);

    /* Open the input and output files. */
    FILE* const fin  = fopen_orDie(fname, "rb");
    FILE* const fout = fopen_orDie(outName, "wb");
    /* Create the input and output buffers.
     * They may be any size, but we recommend using these functions to size them.
     * Performance will only suffer significantly for very tiny buffers.
     */
    size_t const buffInSize = ZSTD_CStreamInSize();
    void*  const buffIn  = malloc_orDie(buffInSize);
    size_t const buffOutSize = ZSTD_CStreamOutSize();
    void*  const buffOut = malloc_orDie(buffOutSize);

    /* Create the context. */
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    CHECK(cctx != NULL, "ZSTD_createCCtx() failed!");

    /* Set any parameters you want.
     * Here we set the compression level, and enable the checksum.
     */
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, cLevel) );
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1) );
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, nbThreads);

    /* This loop read from the input file, compresses that entire chunk,
     * and writes all output produced to the output file.
     */
    size_t const toRead = buffInSize;
    for (;;) {
        size_t read = fread_orDie(buffIn, toRead, fin);
        /* Select the flush mode.
         * If the read may not be finished (read == toRead) we use
         * ZSTD_e_continue. If this is the last chunk, we use ZSTD_e_end.
         * Zstd optimizes the case where the first flush mode is ZSTD_e_end,
         * since it knows it is compressing the entire source in one pass.
         */
        int const lastChunk = (read < toRead);
        ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;
        /* Set the input buffer to what we just read.
         * We compress until the input buffer is empty, each time flushing the
         * output.
         */
        ZSTD_inBuffer input = { buffIn, read, 0 };
        int finished;

        do {
            /* Compress into the output buffer and write all of the output to
             * the file so we can reuse the buffer next iteration.
             */
            ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
            size_t const remaining = ZSTD_compressStream2(cctx, &output , &input, mode);
            CHECK_ZSTD(remaining);
            fwrite_orDie(buffOut, output.pos, fout);
            /* If we're on the last chunk we're finished when zstd returns 0,
             * which means its consumed all the input AND finished the frame.
             * Otherwise, we're finished when we've consumed all the input.
             */
            finished = lastChunk ? (remaining == 0) : (input.pos == input.size);
        } while (!finished);
        CHECK(input.pos == input.size,
              "Impossible: zstd only returns 0 when the input is completely consumed!");

        if (lastChunk) {
            break;
        }
    }

    ZSTD_freeCCtx(cctx);
    fclose_orDie(fout);
    fclose_orDie(fin);
    free(buffIn);
    free(buffOut);
}
#endif

//For passing data into pthreads
typedef struct pthreadWrapper {
    int id;
    ZSTD_CCtx* context;
    char* inPtr; //Read pointer in input buffer
    char* inSize;
    char* outPtr; //Write pointer in output buffer
    char* outSize;
    char* output_pos;
    int cLevel; // Compression level
} pthreadWrapper_t;

static void pthreadCompressor(void* args) {
    struct pthreadWrapper* ptw = (struct pthreadWrapper*)args;
    printf("Hello World! It's me, thread #%d!\n", ptw->id);

    /* Set the input buffer to what we just read.
         * We compress until the input buffer is empty, each time flushing the
         * output.
         */
        ZSTD_inBuffer input = { ptw->inPtr, ptw->inSize, 0 };

        /*printf("Input buffer contents:");
        for(int i = 0; i < read; i++) {
            printf("%c", *testptr);
            testptr++;
        }
        printf("\n");*/


            /* Compress into the output buffer and write all of the output to
             * the file so we can reuse the buffer next iteration.
             */
            ZSTD_outBuffer output = { ptw->outPtr, ptw->outSize, 0 };
            size_t const remaining = ZSTD_compressStream2(ptw->context, &output , &input, ptw->cLevel);
            CHECK_ZSTD(remaining);
            ptw->output_pos = output.pos;

        CHECK(input.pos == input.size,
              "Impossible: zstd only returns 0 when the input is completely consumed!");

    pthread_exit(NULL);
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
    size_t const buffInSize = ZSTD_CStreamInSize();
    void*  const buffIn  = malloc_orDie(buffInSize);
    size_t const buffOutSize = ZSTD_CStreamOutSize();
    void*  const buffOut = malloc_orDie(buffOutSize);

    /* Create the context. */
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    CHECK(cctx != NULL, "ZSTD_createCCtx() failed!");

    /* Set any parameters you want.
     * Here we set the compression level, and enable the checksum.
     */
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, cLevel) );
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1) );
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, nbThreads);

//LOOP MAIN THREAD: READ A CHUNK OF THE FILE
    size_t const toRead = 16*1024;
    char* testptr;
    int actives = 0;
    int i = 0;

    for (;;) {
        //printf("%d\n", i);
        testptr = buffIn;
        size_t read = fread_orDie(buffIn, toRead, fin);
        /* Select the flush mode.
         * If the read may not be finished (read == toRead) we use
         * ZSTD_e_continue. If this is the last chunk, we use ZSTD_e_end.
         * Zstd optimizes the case where the first flush mode is ZSTD_e_end,
         * since it knows it is compressing the entire source in one pass.
         */
        int const lastChunk = (read < toRead);
        ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;

//LOOP MAIN THREAD: MAKE THREAD FOR CURRENT CHUNK

        if(read > 0) {
            printf("Read %d chars\n", read);
            actives++;

            struct pthreadWrapper ptw;
            ptw.id = i;
            ptw.context = cctx;
            ptw.inPtr = (char*)buffIn;
            ptw.inSize = read;
            ptw.outPtr = (char*)buffOut;
            ptw.outSize = buffOutSize;
            ptw.cLevel = cLevel;

            wrappers[i] = ptw;
            pthread_create(pthreads+i, NULL, pthreadCompressor, (void*)(&wrappers[i]));
        }

        if(!(actives < nbThreads)) {
            for(int j = 0; j < nbThreads; j++) {
                pthread_join(pthreads[j], NULL);
                //fwrite_orDie(wrappers[j].outPtr, wrappers[j].output_pos, fout);
            }
        }

        i++;

        if (lastChunk) {
            break;
        }
    }

    //compressFile_orDie(inFilename, outFilename, cLevel, nbThreads);

    ZSTD_freeCCtx(cctx);
    fclose_orDie(fout);
    fclose_orDie(fin);
    free(buffIn);
    free(buffOut);

    free(outFilename);   /* not strictly required, since program execution stops there,
                          * but some static analyzer may complain otherwise */
    return 0;
}