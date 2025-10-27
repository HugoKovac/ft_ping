#include "argp.h"

static const struct argp_option *find_long(const struct argp_option *opts, const char *name)
{
    if (!opts)
        return NULL;
    for (; !(opts->name == NULL && opts->key == 0); ++opts)
    {
        if (opts->name && strcmp(opts->name, name) == 0)
            return opts;
    }
    return NULL;
}

static const struct argp_option *find_short(const struct argp_option *opts, int key)
{
    if (!opts)
        return NULL;
    for (; !(opts->name == NULL && opts->key == 0); ++opts)
    {
        if (opts->key == key)
            return opts;
    }
    return NULL;
}

int argp_parse(const struct argp *argp, int argc, char **argv, int *arg_index, void *input)
{
    struct argp_state state;
    state.argc = argc;
    state.argv = argv;
    state.arg_num = 1;
    state.input = input;

    const struct argp_option *opts = argp ? argp->options : NULL;
    argp_parser_t parser = argp ? argp->parser : NULL;

    int i = 1;
    for (; i < argc; ++i)
    {
        char *s = argv[i];
        if (s[0] != '-' || strcmp(s, "-") == 0)
        {
            break;
        }
        if (strcmp(s, "--") == 0)
        {
            ++i;
            break;
        }
        if (s[1] == '-')
        {
            char *eq = strchr(s + 2, '=');
            char namebuf[256];
            const char *val = NULL;
            if (eq)
            {
                size_t n = eq - (s + 2);
                if (n >= sizeof(namebuf))
                    n = sizeof(namebuf) - 1;
                memcpy(namebuf, s + 2, n);
                namebuf[n] = '\0';
                val = eq + 1;
            }
            else
            {
                strncpy(namebuf, s + 2, sizeof(namebuf));
                namebuf[sizeof(namebuf) - 1] = '\0';
            }
            const struct argp_option *opt = find_long(opts, namebuf);
            if (!opt)
                return ARGP_ERR_UNKNOWN;
            const char *argval = NULL;
            if (opt->has_arg == 1)
            {
                if (val)
                    argval = val;
                else
                {
                    if (i + 1 < argc)
                    {
                        argval = argv[++i];
                    }
                    else
                        return ARGP_ERR_ARG;
                }
            }
            else if (opt->has_arg == 2)
            {
                argval = val;
            }
            else
            {
                argval = NULL;
                if (val)
                {
                    return ARGP_ERR_ARG;
                }
            }
            state.arg_num = i;
            if (parser)
            {
                int r = parser(opt->key, argval, &state);
                if (r)
                    return r;
            }
            continue;
        }
        else
        {
            size_t len = strlen(s);
            size_t j;
            for (j = 1; j < len; ++j)
            {
                int ch = (unsigned char)s[j];
                const struct argp_option *opt = find_short(opts, ch);
                if (!opt)
                    return ARGP_ERR_UNKNOWN;
                const char *argval = NULL;
                if (opt->has_arg == 1)
                {
                    if (j + 1 < len)
                    {
                        argval = &s[j + 1];
                        j = len;
                    }
                    else
                    {
                        if (i + 1 < argc)
                        {
                            argval = argv[++i];
                        }
                        else
                            return ARGP_ERR_ARG;
                    }
                }
                else if (opt->has_arg == 2)
                {
                    if (j + 1 < len)
                        argval = &s[j + 1];
                    else
                        argval = NULL;
                    j = len;
                }
                else
                {
                    argval = NULL;
                }
                state.arg_num = i;
                if (parser)
                {
                    int r = parser(opt->key, argval, &state);
                    if (r)
                        return r;
                }
                if (opt->has_arg)
                    break;
            }
            continue;
        }
    }

    if (arg_index)
        *arg_index = i;
    return ARGP_SUCCESS;
}
