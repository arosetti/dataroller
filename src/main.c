#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <stdbool.h>

#include "file.h"
#include "timer.h"

#include "compress_lzw.h"
#include "decompress_lzw.h"

#define ACTION_UNDEFINED  -1
#define ACTION_COMPRESS    0
#define ACTION_DECOMPRESS  1

#define DEFAULT_DECOMP_NAME "decompressed"

#define PRINT_HUMAN(message, size, sec) printf("%s", message); \
                                        num2human(size, 1000); \
                                        printf("B"); \
                                        if(sec) printf("/s");\
                                        printf(" ( "); \
                                        num2human(size, 1024); \
                                        printf("iB"); \
                                        if(sec) printf("/s");\
                                        printf(" )");

int usage(int argc, char **argv)
{
    fprintf(stderr, "\n"
    "%s %s\nusage: %s [options] ...\n"
    " -d, --decompress  <file>   : decompress file \n"
    " -c, --compress    <file>   : compress file\n"
    " -o, --output      <file>   : output file\n"
    " -r, --ratio       <0..14>  : select compression level\n"
    " -f, --force                : enable overwrite of files\n"
    " -b, --binary               : enable binary mode\n"
    "     --debug                : enable debug messages\n"
    "     --no-verbose           : disable verbose messages\n" /* TODO controlli di output messaggi...*/
    "\n"
    "examples: %s --decompress file.lzw .\n"
    "          %s --ratio 5 --compress file\n",
    PACKAGE_NAME, PACKAGE_VERSION,
    argv[0],argv[0],argv[0]);
    exit(0);
}

int main(int argc, char **argv)
{
    int opt;

    static int no_verbose_flag = 0;
    static int debug_flag = 0;
    static int binary_mode_flag = 0;
    int force_flag = 0;

    int8_t action = ACTION_UNDEFINED;
    uint8_t ratio = 10;
    char *input_file = NULL, *output_file = NULL;
    char *output_dir = NULL;

    while (1)
    {
        static struct option long_options[] =
        {
            {"debug",      no_argument, &debug_flag, 1},
            {"no-verbose", no_argument, &no_verbose_flag, 1},
            {"binary",     no_argument,         0, 'b'},
            {"force",      no_argument,         0, 'f'},
            {"help",       no_argument,         0, 'h'},
            {"compress",   required_argument,   0, 'c'},
            {"decompress", required_argument,   0, 'd'},
            {"output",     required_argument,   0, 'o'},
            {"ratio",      required_argument,   0, 'r'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
 
        opt = getopt_long (argc, argv, "hc:d:r:o:fbs",
                           long_options, &option_index);
 
        if (opt == -1)
            break;
 
        switch (opt)
        {
            case 'h':
                usage(argc,argv);
            break;

            case 'f':
                force_flag = 1;
            break;

            case 'b':
                binary_mode_flag = 1;
            break;

            case 'c':
                 if (action != ACTION_UNDEFINED)
                 {
                    printf ("can't compress and decompress at the same time!\n");
                    usage(argc,argv);
                 }
                action = ACTION_COMPRESS;
                input_file = my_malloc(sizeof(char) * strlen(optarg) + 1); /*TODO check optarg*/
                strcpy(input_file,optarg);
            break;

            case 'd':
                if (action != ACTION_UNDEFINED)
                {
                    printf ("can't compress and decompress at the same time!\n");
                    usage(argc,argv);
                }
                action = ACTION_DECOMPRESS;
                input_file = my_malloc(sizeof(char) * strlen(optarg) + 1); /*TODO check optarg*/
                strcpy(input_file,optarg);
            break;

            case 'o':
                output_file = my_malloc(sizeof(char) * strlen(optarg) + 1); /*TODO check optarg*/
                strcpy(output_file,optarg);
            break;

            case 'r':
                 ratio = atoi(optarg); /*TODO check optarg*/
            break;

            case '?':
                usage(argc,argv);
            break;

            default:
            break;
         }
     }

    if (optind < argc)
    {
        if (optind < argc)
        {
            output_dir = my_malloc(sizeof(char) * strlen(argv[optind]) + 3); /* TODO check */
            strcpy(output_dir,argv[optind++]);

            if (!dir_exists(output_dir))
            {
                fprintf(stderr, "output directory \"%s\" does not exists\n", output_dir);
                free(output_dir);
                output_dir = NULL;
            }

            if (output_dir && strlen(output_dir) > 0 && output_dir[strlen(output_dir) - 1] != '/')
                strcat(output_dir, "/");
        }

        while (optind < argc)
            printf("useless tail options: %s ", argv[optind++]);
        putchar('\n');
    }

    timer tm;
    uint32_t size_a, size_b;
    double time_diff;

    mlockall(MCL_CURRENT | MCL_FUTURE);

    if (input_file && !file_exists(input_file))
    {
        fprintf(stderr, "file \"%s\" does not exists!\n", input_file);
        goto end_main;
    }

    switch (action)
    {
        case ACTION_UNDEFINED:
            fprintf(stderr,"missing required arguments, --compress or --decompress\n");
            usage(argc,argv);
        break;

        case ACTION_COMPRESS:
            size_a = file_size(input_file);

            if (!output_file)
            {
                output_file = my_malloc(strlen(input_file) * sizeof(char) + 5); /* TODO check strlen size ... */
                strcpy(output_file, input_file);
                strcpy((output_file + strlen(input_file)), ".lzw");
            }

            if (output_file && !force_flag && file_exists(output_file))
            {
                fprintf(stderr, "file \"%s\" already exists, use --force option.\n", output_file);
                goto end_main;
            }

            if (!size_a)
            {
                fprintf(stderr, "file \"%s\" is empty.\n", output_file);
                goto end_main;
            }

            printf("* filename            : %s\n", input_file);
            printf("* ratio               : %d\n", ratio);
            printf("* binary mode         : %d\n", binary_mode_flag);
            #ifdef USE_TRUNCATE_BIT_ENCODING
            printf("* encoding            : truncate bit\n");
            #else
            printf("* encoding            : standard\n");
            #endif
            #ifdef USE_INLINE
            printf("* inlining            : enabled\n");
            #else
            printf("* inlining            : disabled\n");
            #endif
            #ifdef USE_TRIE
            printf("* dictionary method   : trie\n");
            #else
            printf("* dictionary method   : hash\n");
            #endif

            PRINT_HUMAN("* uncompressed size   : ", size_a, 0);

            printf("\n\ncompressing.... \"%s\" => \"%s\" \n\n", input_file, output_file);

            timer_start(&tm);
            compress_lzw(input_file, output_file, ratio, binary_mode_flag);
            timer_stop(&tm);
            printf("\n* elapsed time        : ");
            time_diff = timer_diff(&tm);
            time2human(time_diff);

            PRINT_HUMAN("* speed               : ", (double)size_a / time_diff, 1);
            printf("\n");

            size_b = file_size(output_file);
            printf("\n* compression ratio   : %f%%\n",  (100 * (1 - (float) size_b / (float) size_a)) );
            PRINT_HUMAN("* compressed size     : ", size_b, 0);
            printf("\n");

        break;

        case ACTION_DECOMPRESS:
            size_a = file_size(input_file);

            if (!output_file)
            {
                if ((strlen(input_file) > 4) && (strncmp((input_file + strlen(input_file) - 4) , ".lzw", 4) == 0))
                {
                    output_file = my_malloc(strlen(input_file) * sizeof(char) - 4);
                    strncpy(output_file, input_file, strlen(input_file) - 4);
                }
                else
                {
                    output_file = my_malloc(strlen(DEFAULT_DECOMP_NAME) * sizeof(char));
                    strcpy(output_file, DEFAULT_DECOMP_NAME);
                }
            }

            if (output_file && !force_flag && file_exists(output_file))
            {
                fprintf(stderr, "file \"%s\" already exists, use --force option.\n", output_file);
                goto end_main;
            }

            if (!size_a)
            {
                fprintf(stderr, "file \"%s\" is empty.\n", output_file);
                goto end_main;
            }

            printf("* filename            : %s\n", input_file);
            #ifdef USE_TRUNCATE_BIT_ENCODING
            printf("* encoding            : truncate bit\n");
            #else
            printf("* encoding            : standard\n");
            #endif
            #ifdef USE_INLINE
            printf("* inlining            : enabled\n");
            #else
            printf("* inlining            : disabled\n");
            #endif
    
            PRINT_HUMAN("* compressed size     : ", size_a, 0);

            printf("\n\ndecompressing.... \"%s\" => \"%s\" \n\n", input_file, output_file);
            timer_start(&tm);
            if (decompress_lzw(input_file, output_file) != 0)
            {
                printf("error, something has gone wrong...\n");
                goto end_main;
            }
            timer_stop(&tm);
            printf("\n* elapsed time        : ");
            time_diff = timer_diff(&tm);
            time2human(time_diff);

            size_b = file_size(output_file);

            PRINT_HUMAN("* speed               : ", (double)size_a / time_diff, 1);
            printf("\n");

            PRINT_HUMAN("* decompressed size   : ", size_b, 0);
            printf("\n");
        break;
    }

    end_main:

    if (output_file)
        free(output_file);
    if (input_file)
        free(input_file);
    if (output_dir)
        free(output_dir);

    return 0;
}


