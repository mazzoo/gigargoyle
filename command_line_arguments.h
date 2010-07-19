/*
 * gigargoyle
 *
 * a nighttime composition in opensource
 *
 * this is part of the 2010 binkenlights installation
 * in Giesing, Munich, Germany which is called
 *
 *   a.c.a.b. - all colors are beautiful
 *
 * the installation is run by the Chaos Computer Club Munich
 * as part of the puerto giesing
 * 
 *
 * license:
 *          GPL v2, see the file LICENSE
 * authors:
 *          Raffael Mancini <raffael.mancini@hcl-club.lu>
 *
 */

#ifndef COMMAND_LINE_ARGUMENTS_H
#define COMMAND_LINE_ARGUMENTS_H

#include <argp.h>

struct arguments {
  int pretend;
  int foreground;
  int port_qm;
  int port_is;
  int port_web;
  int acab_x;
  int acab_y;
  char *row_0_uart;
  char *row_1_uart;
  char *row_2_uart;
  char *row_3_uart;
  char *pid_file;
  char *log_file;
};

void init_arguments(struct arguments *args);
error_t parse_opt(int key, char *arg, struct argp_state *state);

#endif
