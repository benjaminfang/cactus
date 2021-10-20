/*
 * paf_dechunk: Used with fasta_chunk, switches the coordinates of the pafs
 * to use the original sequences passed into fasta_chunk.
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "paf.h"
#include <getopt.h>
#include <time.h>
#include "bioioC.h"
#include "sonLib.h"

 void usage() {
     fprintf(stderr, "paf_dechunk [options], version 0.1\n");
     fprintf(stderr, "Used in conjunction with fasta_chunk.\n"
                     "Modifies paf coordinates to remove the chunk coordinate name encoding created by fasta_chunk.\n");
     fprintf(stderr, "-i --inputFile : Input paf file to invert. If not specified reads from stdin\n");
     fprintf(stderr, "-o --outputFile : Output paf file. If not specified outputs to stdout\n");
     fprintf(stderr, "-l --logLevel : Set the log level\n");
     fprintf(stderr, "-h --help : Print this help message\n");
 }

static void convertCoordinatesP(char **contig, int64_t *start, int64_t *end, int64_t *length) {
    stList *attributes = fastaDecodeHeader(*contig);
    //Decode attributes
    int64_t startP;
    int64_t i = sscanf((const char *) stList_peek(attributes), "%" PRIi64 "", &startP);
    (void) i;
    free(stList_pop(attributes));
    assert(i == 1);
    i = sscanf((const char *) stList_peek(attributes), "%" PRIi64 "", length);
    free(stList_pop(attributes));
    assert(i == 1);
    //Now relabel attributes
    free(*contig);
    *contig = fastaEncodeHeader(attributes);
    stList_destruct(attributes);
    *start = *start + startP;
    *end = *end + startP;
}

void paf_dechunk(Paf *paf) {
    convertCoordinatesP(&paf->query_name, &paf->query_start, &paf->query_end, &paf->query_length);
    convertCoordinatesP(&paf->target_name, &paf->target_start, &paf->target_end, &paf->target_length);
 }

int main(int argc, char *argv[]) {
     time_t startTime = time(NULL);

     /*
      * Arguments/options
      */
     char *logLevelString = NULL;
     char *inputFile = NULL;
     char *outputFile = NULL;

     ///////////////////////////////////////////////////////////////////////////
     // Parse the inputs
     ///////////////////////////////////////////////////////////////////////////

     while (1) {
         static struct option long_options[] = { { "logLevel", required_argument, 0, 'l' },
                                                 { "inputFile", required_argument, 0, 'i' },
                                                 { "outputFile", required_argument, 0, 'o' },
                                                 { "help", no_argument, 0, 'h' },
                                                 { 0, 0, 0, 0 } };

         int option_index = 0;
         int64_t key = getopt_long(argc, argv, "l:i:o:h", long_options, &option_index);
         if (key == -1) {
             break;
         }

         switch (key) {
             case 'l':
                 logLevelString = optarg;
                 break;
             case 'i':
                 inputFile = optarg;
                 break;
             case 'o':
                 outputFile = optarg;
                 break;
             case 'h':
                 usage();
                 return 0;
             default:
                 usage();
                 return 1;
         }
     }

     //////////////////////////////////////////////
     //Log the inputs
     //////////////////////////////////////////////

     st_setLogLevelFromString(logLevelString);
     st_logInfo("Input file string : %s\n", inputFile);
     st_logInfo("Output file string : %s\n", outputFile);

     //////////////////////////////////////////////
     // De-chunk the paf
     //////////////////////////////////////////////

     FILE *input = inputFile == NULL ? stdin : fopen(inputFile, "r");
     FILE *output = outputFile == NULL ? stdout : fopen(outputFile, "w");

     Paf *paf;
     while((paf = paf_read(input)) != NULL) {
         paf_dechunk(paf);
         paf_check(paf);
         paf_write(paf, output);
         paf_destruct(paf);
     }

     //////////////////////////////////////////////
     // Cleanup
     //////////////////////////////////////////////

     if(inputFile != NULL) {
         fclose(input);
     }
     if(outputFile != NULL) {
         fclose(output);
     }

     st_logInfo("Paf dechunk is done!, %" PRIi64 " seconds have elapsed\n", time(NULL) - startTime);

     //while(1);
     //assert(0);

     return 0;
 }
