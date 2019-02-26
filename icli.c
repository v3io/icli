/*
 * Copyright 2019 Iguazio.io Systems Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License") with
 * an addition restriction as set forth herein. You may not use this
 * file except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * In addition, you may not use the software for any purposes that are
 * illegal under applicable law, and the grant of the foregoing license
 * under the Apache 2.0 license is conditioned upon your compliance with
 * such restriction.
 */

#include "icli.h"

#define _GNU_SOURCE

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/queue.h>

#include <editline/readline.h>

#define ANSI_BLACK_NORMAL "\x1b[30m"
#define ANSI_RED_NORMAL "\x1b[31m"
#define ANSI_GREEN_NORMAL "\x1b[32m"
#define ANSI_YELLOW_NORMAL "\x1b[33m"
#define ANSI_BLUE_NORMAL "\x1b[34m"
#define ANSI_MAGENTA_NORMAL "\x1b[35m"
#define ANSI_CYAN_NORMAL "\x1b[36m"
#define ANSI_WHITE_NORMAL "\x1b[37m"
#define ANSI_RESET "\x1b[0m"

/* A structure which contains information on the commands this program
   can understand. */
struct icli_command {
    LIST_ENTRY(icli_command) cmd_list_entry;
    char *name; /* User printable name of the function. */
    char *short_name;
    icli_cmd_func_t *func; /* Function to call to do the job. */
    char *doc; /* Documentation for this function.  */
    LIST_HEAD(, icli_command) cmd_list;
    size_t n_cmds;
    struct icli_command *parent;
    int argc;
    struct icli_arg_val **argv;
    int max_name_len;
    int name_len;
    char *prompt_line;
};

struct icli {
    void *user_data;
    /* When non-zero, this means the user is done using this program. */
    bool done;
    struct icli_command *root_cmd;
    struct icli_command *curr_cmd;
    char *curr_prompt;
    const char *prompt;

    int rows;
    int cols;
    int curr_row;

    struct icli_command *curr_completion_cmd;
    int curr_completion_arg;
};

static struct icli icli;

#define array_len(_array) (sizeof(_array) / sizeof((_array)[0]))

#define UNUSED __attribute__((__unused__))

/* Strip whitespace from the start and end of STRING.  Return a pointer
   into STRING. */
static char *stripwhite(char *string)
{
    register char *s, *t;

    for (s = string; isspace(*s); s++)
        ;

    if (*s == 0)
        return (s);

    t = s + strlen(s) - 1;
    while (t > s && isspace(*t))
        t--;
    *++t = '\0';

    return s;
}

/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
static struct icli_command *icli_find_command(char *name)
{
    struct icli_command *it;

    LIST_FOREACH(it, &icli.curr_cmd->cmd_list, cmd_list_entry)
    {
        if (strcmp(name, it->name) == 0)
            return it;
    }

    return ((struct icli_command *)NULL);
}

static void icli_cat_command(struct icli_command *curr)
{
    if (!curr->parent)
        return;

    icli_cat_command(curr->parent);

    if (curr->name) {
        strcat(icli.curr_prompt, "(");

        if (curr->prompt_line) {
            strcat(icli.curr_prompt, curr->prompt_line);
        } else if (curr->short_name) {
            strcat(icli.curr_prompt, curr->short_name);
        } else if (curr->name) {
            strcat(icli.curr_prompt, curr->name);
        }
        strcat(icli.curr_prompt, ")");
    }
}

static void icli_build_prompt(struct icli_command *command)
{
    /*'\0' + prompt + '>' + ' ' */
    size_t buf_sz = 1 + strlen(icli.prompt) + 2;

    struct icli_command *curr = command;
    while (curr) {
        if (curr->prompt_line) {
            /*command + '(' + ')' */
            buf_sz += 2 + strlen(curr->prompt_line);
        } else if (curr->short_name) {
            /*command + '(' + ')' */
            buf_sz += 2 + strlen(curr->short_name);
        } else if (curr->name) {
            /*command + '(' + ')' */
            buf_sz += 2 + strlen(curr->name);
        }
        curr = curr->parent;
    }

    free((void *)icli.curr_prompt);
    icli.curr_prompt = malloc(buf_sz);

    strncpy(icli.curr_prompt, icli.prompt, buf_sz);
    icli_cat_command(command);

    strcat(icli.curr_prompt, "> ");
}

static int icli_parse_line(char *line, char **cmd, char *argv[], int argc)
{
    int n_args = 0;
    int i = 0;
    char *tmp;

    /* Isolate the command word. */
    while (line[i] && isspace(line[i]))
        i++;
    tmp = &line[i];

    while (line[i] && !isspace(line[i]))
        i++;

    if (line[i])
        line[i++] = '\0';

    *cmd = tmp;

    /* Get argument to command, if any. */
    while (line[i]) {
        while (isspace(line[i]))
            i++;

        if (line[i] && n_args < argc) {
            argv[n_args++] = &line[i];
        }

        while (line[i] && !isspace(line[i]))
            i++;

        if (line[i])
            line[i++] = '\0';
    }

    return n_args;
}

static int icli_set_command_prompt(struct icli_command *cmd, char *argv[], int argc)
{
    size_t bufsz = 1;
    const char *name = NULL;

    free(cmd->prompt_line);
    cmd->prompt_line = NULL;

    if (cmd->short_name) {
        bufsz += strlen(cmd->short_name);
        name = cmd->short_name;
    } else if (cmd->name) {
        bufsz += strlen(cmd->name);
        name = cmd->name;
    }

    for (int i = 0; i < argc; ++i) {
        bufsz += strlen(argv[i]) + 1;
    }

    cmd->prompt_line = malloc(bufsz);
    if (!cmd->prompt_line)
        return -1;

    strcpy(cmd->prompt_line, name);
    for (int i = 0; i < argc; ++i) {
        strcat(cmd->prompt_line, " ");
        strcat(cmd->prompt_line, argv[i]);
    }

    return 0;
}

static int icli_execute_line(char *line)
{
    struct icli_command *command;
    static char *argv[ICLI_ARGS_MAX];
    char *cmd;

    int argc = icli_parse_line(line, &cmd, argv, array_len(argv));

    command = icli_find_command(cmd);

    if (!command) {
        icli_err_printf("%s: No such command\n", cmd);
        return -1;
    }

    if (!command->func && argc) {
        icli_err_printf("Command %s does not accept arguments\n", cmd);
        return -1;
    }

    if (command->func) {
        if (command->argc != ICLI_ARGS_DYNAMIC) {
            if (command->argc != argc) {
                icli_err_printf("Command %s accepts exactly %d arguments. %d were provided\n",
                                cmd,
                                command->argc,
                                argc);
                return -1;
            }

            if (command->argv) {
                for (int i = 0; i < command->argc; ++i) {
                    struct icli_arg_val *vals = command->argv[i];
                    bool found = false;

                    if (!vals)
                        continue;

                    while (vals->val) {
                        if (strcmp(vals->val, argv[i]) == 0) {
                            found = true;
                            break;
                        }
                        ++vals;
                    }

                    if (!found) {
                        icli_err_printf("Command %s %d argument invalid: %s. Possible values:\n", cmd, i, argv[i]);

                        int printed = 0;
                        for (vals = command->argv[i]; vals->val; ++vals) {
                            /* Print in six columns. */
                            if (printed == 6) {
                                printed = 0;
                                icli_err_printf("\n");
                            }

                            icli_err_printf("%s\t", vals->val);
                            printed++;
                        }

                        if (printed)
                            icli_err_printf("\n");

                        return -1;
                    }
                }
            }
        }

        icli_set_command_prompt(command, argv, argc);

        /* Call the function. */
        enum icli_ret ret = ((*(command->func))(argv, argc, icli.user_data));

        switch (ret) {
        case ICLI_OK:
            break;
        case ICLI_ERR_ARG:
            icli_err_printf("Argument error\n");
            free(command->prompt_line);
            command->prompt_line = NULL;
            return -1;
            break;
        case ICLI_ERR:
            icli_err_printf("Error\n");
            free(command->prompt_line);
            command->prompt_line = NULL;
            return -1;
            break;
        }
    }

    if (command->n_cmds) {
        icli.curr_cmd = command;
        icli_build_prompt(command);
    }

    return 0;
}

/* Generator function for command argument completion.  STATE lets us
   know whether to start from scratch; without any state
   (i.e. STATE == 0), then we start at the top of the list. */
static char *icli_command_arg_generator(const char *text, int state)
{
    static size_t len;
    static struct icli_arg_val *vals;
    const char *name;

    /* If this is a new word to complete, initialize now.  This
       includes saving the length of TEXT for efficiency, and
       initializing the index variable to 0. */
    if (!state) {
        if (text)
            len = strlen(text);
        else
            len = 0;

        if (len)
            vals = icli.curr_completion_cmd->argv[icli.curr_completion_arg - 1];
        else
            vals = icli.curr_completion_cmd->argv[icli.curr_completion_arg];
    }

    /* Return the next name which partially matches from the
       argument list. */
    while (vals && vals->val) {
        name = vals->val;

        ++vals;
        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }

    /* If no names matched, then return NULL. */
    return (char *)NULL;
}

/* Generator function for command completion.  STATE lets us
   know whether to start from scratch; without any state
   (i.e. STATE == 0), then we start at the top of the list. */
static char *icli_command_generator(const char *text, int state)
{
    static struct icli_command *list_entry;
    static size_t len;
    char *name;

    /* If this is a new word to complete, initialize now.  This
       includes saving the length of TEXT for efficiency, and
       initializing the index variable to 0. */
    if (!state) {
        list_entry = LIST_FIRST(&icli.curr_cmd->cmd_list);
        if (text)
            len = strlen(text);
        else
            len = 0;
    }

    /* Return the next name which partially matches from the
       command list. */
    while (list_entry) {
        name = list_entry->name;
        list_entry = LIST_NEXT(list_entry, cmd_list_entry);

        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }

    /* If no names matched, then return NULL. */
    return (char *)NULL;
}

/* Attempt to complete on the contents of TEXT.  START and END
   bound the region of rl_line_buffer that contains the word to
   complete.  TEXT is the word to complete.  We can use the entire
   contents of rl_line_buffer in case we want to do some simple
   parsing.  Return the array of matches, or NULL if there aren't any. */
static char **icli_completion(const char *text, int start, int end UNUSED)
{
    char **matches;

    static char *argv[ICLI_ARGS_MAX];
    char *cmd;

    static char tmp[4096];
    memcpy(tmp, rl_line_buffer, (size_t)rl_end);
    tmp[rl_end] = '\0';
    stripwhite(tmp);

    int argc = icli_parse_line(tmp, &cmd, argv, array_len(argv));

    /* Don't do filename completion even if our generator finds no matches. */
    rl_attempted_completion_over = 1;

    matches = (char **)NULL;

    icli.curr_completion_cmd = NULL;
    icli.curr_completion_arg = 0;

    /* If this word is at the start of the line, then it is a command
     * to complete */
    if (start == 0) {
        matches = completion_matches((char *)text, icli_command_generator);
        /* matches = rl_completion_matches (text, icli_command_generator); */
    } else {
        struct icli_command *command = icli_find_command(cmd);
        if (command && command->argc != ICLI_ARGS_DYNAMIC && command->argc) {
            if (argc <= command->argc && command->argv) {
                icli.curr_completion_cmd = command;
                icli.curr_completion_arg = argc;
                matches = completion_matches((char *)text, icli_command_arg_generator);
            }
        }
    }

    return matches;
}

static enum icli_ret icli_history(char *argv[] UNUSED, int argc UNUSED, void *context UNUSED)
{
    HIST_ENTRY *he;

    /* rewind history */
    while (previous_history())
        ;

    for (he = current_history(); he != NULL; he = next_history()) {
        // icli_printf("%5d  %s\n", *((int*)he->data) - 1, he->line);
        icli_printf("%s\n", he->line);
    }

    return ICLI_OK;
}

static enum icli_ret icli_end(char *argv[] UNUSED, int argc UNUSED, void *context UNUSED)
{
    icli.curr_cmd = icli.curr_cmd->parent;

    if (NULL == icli.curr_cmd) {
        icli.curr_cmd = icli.root_cmd;
    }

    icli_build_prompt(icli.curr_cmd);

    return ICLI_OK;
}

/* Print out help for ARG, or for all of the commands if ARG is
   not present. */
static enum icli_ret icli_help(char *argv[], int argc, void *context UNUSED)
{
    int printed = 0;
    struct icli_command *it;

    if (argc > 1)
        return ICLI_ERR_ARG;

    icli_printf("Available commands:\n");

    LIST_FOREACH(it, &icli.curr_cmd->cmd_list, cmd_list_entry)
    {
        if (0 == argc || (strcmp(argv[0], it->name) == 0)) {
            icli_printf("    %-*s : %s\n", icli.curr_cmd->max_name_len, it->name, it->doc);
            printed++;
        }
    }

    if (!printed) {
        icli_err_printf("No commands match '%s'.  Possibilities are:\n", argv[0]);

        LIST_FOREACH(it, &icli.curr_cmd->cmd_list, cmd_list_entry)
        {
            /* Print in six columns. */
            if (printed == 6) {
                printed = 0;
                icli_err_printf("\n");
            }

            icli_err_printf("%s\t", it->name);
            printed++;
        }

        if (printed)
            icli_err_printf("\n");
    }
    return ICLI_OK;
}

static enum icli_ret icli_quit(char *argv[] UNUSED, int argc UNUSED, void *context UNUSED)
{
    icli.done = true;
    return ICLI_OK;
}

static int icli_init_default_cmds(struct icli_command *parent)
{
    struct icli_command_params params[] =
        {{.parent = parent,
          .name = "help",
          .command = icli_help,
          .argc = ICLI_ARGS_DYNAMIC,
          .help = "Show available commands or show help of a specific command"},
         {.parent = parent, .name = "?", .command = icli_help, .argc = ICLI_ARGS_DYNAMIC, .help = "Synonym for 'help'"},
         {.parent = parent,
          .name = "history",
          .command = icli_history,
          .help = "Show a list of previously run commands"}};

    return icli_register_commands(params, array_len(params));
}

static void icli_clean_command(struct icli_command *cmd)
{
    while (!LIST_EMPTY(&cmd->cmd_list)) {
        struct icli_command *it = LIST_FIRST(&cmd->cmd_list);
        LIST_REMOVE(it, cmd_list_entry);
        icli_clean_command(it);
    }

    free(cmd->name);
    cmd->name = NULL;
    free(cmd->short_name);
    cmd->short_name = NULL;
    free(cmd->doc);
    cmd->doc = NULL;
    free(cmd->prompt_line);
    cmd->prompt_line = NULL;

    if (cmd->argc && cmd->argv) {
        for (int j = 0; j < cmd->argc; ++j) {
            for (struct icli_arg_val *val = cmd->argv[j]; val && val->val; ++val) {
                free((void *)val->val);
                val->val = NULL;
            }
            free(cmd->argv[j]);
            cmd->argv[j] = NULL;
        }

        free(cmd->argv);
        cmd->argv = NULL;
    }

    cmd->argc = 0;

    free(cmd);
}

int icli_register_commands(struct icli_command_params *params, int n_commands)
{
    int ret = 0;

    for (int i = 0; i < n_commands; ++i) {
        ret = icli_register_command(&params[i], NULL);
        if (ret)
            return ret;
    }

    return ret;
}

int icli_register_command(struct icli_command_params *params, struct icli_command **out_command)
{
    bool need_end = true;
    struct icli_command *parent, *it;
    int ret = 0;

    if (out_command)
        *out_command = NULL;

    if (!params)
        return -1;

    if (!params->name || !(*params->name) || !params->help || !(*params->help))
        return -1;

    if (!params->command && params->argc != 0)
        return -1;

    if (params->argv && 0 == params->argc)
        return -1;

    parent = params->parent;

    if (NULL == parent) {
        need_end = false;
        parent = icli.root_cmd;
    }

    LIST_FOREACH(it, &parent->cmd_list, cmd_list_entry)
    {
        if (strcmp(it->name, params->name) == 0) {
            return -1;
        }
    }

    struct icli_command *cmd = calloc(1, sizeof(struct icli_command));
    if (NULL == cmd)
        return -1;

    LIST_INIT(&cmd->cmd_list);
    cmd->name = strdup(params->name);
    cmd->name_len = (int)strlen(cmd->name);
    cmd->doc = strdup(params->help);
    cmd->func = params->command;
    cmd->parent = parent;
    cmd->argc = params->argc;
    if (params->short_name)
        cmd->short_name = strdup(params->short_name);

    if (params->argv) {
        cmd->argv = calloc((size_t)cmd->argc, sizeof(struct icli_arg_val *));
        if (!cmd->argv) {
            ret = -1;
            icli_clean_command(cmd);
            goto out;
        }

        for (int i = 0; i < cmd->argc; ++i) {
            int n_vals = 0;
            for (struct icli_arg_val *val = params->argv[i]; val && val->val; ++val, ++n_vals)
                ;

            if (n_vals) {
                struct icli_arg_val *vals = calloc((size_t)(n_vals + 1), sizeof(struct icli_arg_val));
                if (!vals) {
                    ret = -1;
                    icli_clean_command(cmd);
                    goto out;
                }

                cmd->argv[i] = vals;

                for (int j = 0; params->argv[i][j].val; ++j) {
                    vals[j].val = strdup(params->argv[i][j].val);
                    if (!vals[j].val) {
                        ret = -1;
                        icli_clean_command(cmd);
                        goto out;
                    }
                }
            }
        }
    }

    if (cmd->name_len > parent->max_name_len)
        parent->max_name_len = cmd->name_len;

    ++parent->n_cmds;

    if (1 == parent->n_cmds && need_end) {
        struct icli_command_params param = {.parent = parent,
                                            .name = "end",
                                            .command = icli_end,
                                            .help = "Exit to upper level"};
        ret = icli_register_command(&param, NULL);
        if (ret) {
            --parent->n_cmds;
            icli_clean_command(cmd);
            goto out;
        }
        ret = icli_init_default_cmds(parent);
        if (ret) {
            --parent->n_cmds;
            icli_clean_command(cmd);
            goto out;
        }
    }

    LIST_INSERT_HEAD(&parent->cmd_list, cmd, cmd_list_entry);

    if (out_command)
        *out_command = cmd;

out:
    return ret;
}

int icli_init(struct icli_params *params)
{
    memset(&icli, 0, sizeof(icli));

    icli.root_cmd = calloc(1, sizeof(struct icli_command));
    if (!icli.root_cmd)
        return -1;

    icli.user_data = params->user_data;
    icli.curr_cmd = icli.root_cmd;
    icli.prompt = strdup(params->prompt);
    LIST_INIT(&icli.curr_cmd->cmd_list);

    /* Allow conditional parsing of the ~/.inputrc file. */
    rl_readline_name = strdup(params->app_name);

    /* Tell the completer that we want a crack first. */
    rl_attempted_completion_function = icli_completion;
    rl_completion_entry_function = icli_command_generator;

    using_history();
    stifle_history(params->history_size);

    rl_get_screen_size(&icli.rows, &icli.cols);

    icli_build_prompt(icli.curr_cmd);

    struct icli_command_params param = {.name = "quit", .command = icli_quit, .help = "Quit interactive shell"};
    int ret = icli_register_command(&param, NULL);
    if (ret) {
        goto err;
    }

    ret = icli_init_default_cmds(NULL);
    if (ret) {
        goto err;
    }

    return 0;
err:
    icli_cleanup();
    return ret;
}

void icli_cleanup(void)
{
    icli_clean_command(icli.root_cmd);

    rl_callback_handler_remove();

    free(rl_readline_name);
    rl_readline_name = "";

    free((void *)icli.prompt);
    free((void *)icli.curr_prompt);
}

void icli_run(void)
{
    char *line, *s;

    /* Loop reading and executing lines until the user quits. */
    while (!icli.done) {
        line = readline(icli.curr_prompt);

        if (!line)
            break;

        /* Remove leading and trailing whitespace from the line.
           Then, if there is anything left, add it to the history list
           and execute it. */
        s = stripwhite(line);

        if (*s) {
            char *expansion;
            int result = history_expand(s, &expansion);

            if (result < 0) {
                icli_err_printf("%s\n", expansion);
            } else if (result == 2) {
                icli_printf("%s\n", expansion);
            } else {
                add_history(expansion);
                icli_execute_line(expansion);
            }
            free(expansion);
        }

        free(line);
    }
}

void icli_printf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void icli_err_printf(const char *format, ...)
{
    va_list args;

    printf(ANSI_RED_NORMAL);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(ANSI_RESET);
}

void icli_set_prompt(const char *prompt)
{
    free((void *)icli.prompt);
    icli.prompt = strdup(prompt);
    icli_build_prompt(icli.curr_cmd);
}

static int icli_print_command_to_dot(struct icli_command *cmd, FILE *out)
{
    struct icli_command *it;
    int ret;

    if (!LIST_EMPTY(&cmd->cmd_list)) {
        if (cmd->name)
            ret = fprintf(out, "\"%s\" -> { ", cmd->name);
        else
            ret = fprintf(out, "\"root\" -> { ");
        if (ret < 0)
            return ret;

        LIST_FOREACH(it, &cmd->cmd_list, cmd_list_entry)
        {
            ret = fprintf(out, "\"%s\" ", it->name);
            if (ret < 0)
                return ret;
        }

        ret = fprintf(out, "};\n");
        if (ret < 0)
            return ret;
    }

    LIST_FOREACH(it, &cmd->cmd_list, cmd_list_entry)
    {
        ret = icli_print_command_to_dot(it, out);
        if (ret < 0)
            return ret;
    }

    return 0;
}

#define DOT_GRAPH_PREFIX "digraph {\n"
#define DOT_GRAPH_POSTFIX "}\n"

int icli_commands_to_dot(const char *fname)
{
    int ret = 0;
    size_t written;
    FILE *out = fopen(fname, "w");
    if (!out) {
        return -1;
    }

    written = fwrite(DOT_GRAPH_PREFIX, sizeof(DOT_GRAPH_PREFIX) - 1, 1, out);
    if (written != 1) {
        ret = -1;
        goto out;
    }

    ret = icli_print_command_to_dot(icli.root_cmd, out);
    if (ret)
        goto out;

    written = fwrite(DOT_GRAPH_POSTFIX, sizeof(DOT_GRAPH_POSTFIX) - 1, 1, out);
    if (written != 1) {
        ret = -1;
        goto out;
    }

out:
    fclose(out);

    return ret;
}
