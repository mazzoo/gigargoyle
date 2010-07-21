#include <argp.h>
#include <stdlib.h>

#include "config.h"

#include "command_line_arguments.h"

/* Contains parsed command line arguments */
struct arguments arguments;

void init_arguments(struct arguments *args) {
  args->pretend = 0;
  args->foreground = 0;
  args->row_0_uart = ROW_0_UART;
  args->row_1_uart = ROW_1_UART;
  args->row_2_uart = ROW_2_UART;
  args->row_3_uart = ROW_3_UART;
  args->pid_file = PID_FILE;
  args->log_file = LOG_FILE;
  args->acab_x = ACAB_X;
  args->acab_y = ACAB_Y;
  args->port_qm = PORT_QM;
  args->port_is = PORT_IS;
  args->port_web = PORT_WEB;
}

error_t parse_opt(int key, char *arg, struct argp_state *state) {
  /* Get the INPUT argument from `argp_parse', which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;
  char *endptr;

  switch (key) {
  case 'p':
    arguments->pretend = 1;
    arguments->row_0_uart = "/dev/null";
    arguments->row_1_uart = "/dev/null";
    arguments->row_2_uart = "/dev/null";
    arguments->row_3_uart = "/dev/null";

    break;
  case 'f':
    arguments->foreground = 1;
    break;
  case 'q':
    arguments->port_qm = strtol(arg, &endptr, 0);
    if (arguments->port_qm == 0) {
      argp_error(state, "Could not parse port: %s", arg);
      return ARGP_ERR_UNKNOWN;
    } else
      break;
  case 'i':
    arguments->port_is = strtol(arg, &endptr, 0);
    if (arguments->port_is == 0) {
      argp_error(state, "Could not parse port: %s", arg);
      return ARGP_ERR_UNKNOWN;
    } else
      break;
  case 'w':
    arguments->port_web = strtol(arg, &endptr, 0);
    if (arguments->port_web == 0) {
      argp_error(state, "Could not parse port: %s", arg);
      return ARGP_ERR_UNKNOWN;
    } else
      break;
  case 'x':
    arguments->acab_x = strtol(arg, &endptr, 0);
    if (arguments->acab_x == 0) {
      argp_error(state, "Could not parse int: %s", arg);
      return ARGP_ERR_UNKNOWN;
    } else
      break;
  case 'y':
    arguments->acab_y = strtol(arg, &endptr, 0);
    if (arguments->acab_y == 0) {
      argp_error(state, "Could not parse int: %s", arg);
      return ARGP_ERR_UNKNOWN;
    } else
      break;
  case 127+1:
    arguments->row_0_uart = arg;
    break;
  case 127+2:
    arguments->row_1_uart = arg;
    break;
  case 127+3:
    arguments->row_2_uart = arg;
    break;
  case 127+4:
    arguments->row_3_uart = arg;
    break;
  case 127+5:
    arguments->pid_file = arg;
    break;
  case 'l':
    arguments->log_file = arg;
    break;
  case ARGP_KEY_END:
    if (state->arg_num > 0)
      argp_usage(state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}
