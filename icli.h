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

#pragma once

/**
 * @file
 * @brief icli API
 */

/**
 * Maximum number of arguments for single command
 */
#define ICLI_ARGS_MAX 20

/**
 * Number of arguments is dynamic depending on other arguments
 */
#define ICLI_ARGS_DYNAMIC (-1)

/**
 * Structure to initialize the library instance
 * Note that library instance is global per process (readline limitation)
 */
struct icli_params {
    void *user_data; /**< user provided data to be passed to all commands */
    int history_size; /**< how many commands to keep in history */
    const char *app_name; /**< name of application for interfacing ~/.inputrc */
    const char *prompt; /**< prompt string (will be post-fixed by "> " */
};

/**
 * Return code of command functions
 */
enum icli_ret { ICLI_OK, ICLI_ERR_ARG, ICLI_ERR };

struct icli_command;

/**
 * Command callback function
 */
typedef enum icli_ret icli_cmd_func_t(char *[], int, void *);

/**
 * Argument value
 */
struct icli_arg_val {
    const char *val; /**< Actual value */
};

/**
 * Command registration parameters
 */
struct icli_command_params {
    struct icli_command *parent; /**< parent of the command - root command if NULL is provided */
    const char *name; /**< name of the command - can't be NULL, or empty string */
    icli_cmd_func_t *command; /**< the callback to call - if NULL is passed, command will not accept arguments */
    const char *help; /**< help string for the command - can't be NULL, or empty string */
    int argc; /**< number of arguments to the command @see #ICLI_ARGS_DYNAMIC */
    /** Argument value that are acceptable at each position. NULL means no validation on argument in array. argv can be
     * NULL. in such case no validation is performed */
    struct icli_arg_val **argv;
};

/**
 * Initialize cli engine
 * @param params
 * @return 0 on success
 */
int icli_init(struct icli_params *params);

/**
 * Cleanup cli engine
 */
void icli_cleanup(void);

/**
 * Run the main cli loop
 */
void icli_run(void);

/**
 * Register new command
 * @param params params to initialize with @see icli_command_params()
 * @param[out] out_command where to store resulting command - can be NULL
 * @return 0 on success, !0 on error
 */
int icli_register_command(struct icli_command_params *params, struct icli_command **out_command);

/**
 * Register number of commands
 * @param params the parameter of each command
 * @param n_commands number of commands
 * @return @see icli_register_command()
 */
int icli_register_commands(struct icli_command_params *params, int n_commands);

/**
 * Print output to user. This must be used instead of printf
 * @param format
 * @param ...
 */
void icli_printf(const char *format, ...) __attribute__((__format__(__printf__, 1, 2)));

/**
 * Print error message to user. This must be used instead of printf
 * @param format
 * @param ...
 */
void icli_err_printf(const char *format, ...) __attribute__((__format__(__printf__, 1, 2)));

/**
 * Change the prompt to user
 * @param prompt the new string
 */
void icli_set_prompt(const char *prompt);
