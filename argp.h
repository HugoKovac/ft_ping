#ifndef ARGP_H
#define ARGP_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct argp_state
{
    int argc;
    char **argv;
    int arg_num; 
    void *input;
};

struct argp_option
{
    const char *name;
    int key;         
    int has_arg;     
    const char *doc;
    int group;
};

typedef int (*argp_parser_t)(int key, const char *arg, struct argp_state *state);

struct argp
{
    const struct argp_option *options;
    argp_parser_t parser;
    const char *args_doc;
    const char *doc;
};

enum
{
    ARGP_SUCCESS = 0,
    ARGP_ERR_UNKNOWN = 1,
    ARGP_ERR_ARG = 2
};

int argp_parse(const struct argp *argp, int argc, char **argv, int *arg_index, void *input);

enum
{
    ARGP_NO_ARG = 0,
    ARGP_REQUIRED_ARG = 1,
    ARGP_OPTIONAL_ARG = 2
};

#endif