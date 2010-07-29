/** @file simple_client.c
 *
 * @brief This simple client demonstrates the most basic features of JACK
 * as they would be used by many applications.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <fftw3.h>

#include <jack/jack.h>

#include "gg_simple_client.h"

#define COLS 24
#define ROWS 4
#define FRAME_DURATION 1.0f/30*1e3

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
gg_frame *frame;

int f_to_z(double f) {
  return 13.0*atan(.00076*f)+3.5*atan((f/7500.0)*(f/7500.0));
}

double z_to_f(int z) {
  double map[] = {0, 50, 150, 250, 350, 450, 570, 700, 840, 1000, 1170, 1370, 1600, 1850, 2150, 2500, 2900, 3400, 4000, 4800, 5800, 7000, 8500, 10500, 13500, 20500, 27000};
  return map[z];
}

void interp_color(int ri1, int gi1, int bi1,
                  int ri2, int gi2, int bi2,
                  int *ro, int *go, int *bo,
                  float lambda) {
  *ro = lambda * ri1 + (1-lambda) * ri2;
  *go = lambda * gi1 + (1-lambda) * gi2;
  *bo = lambda * bi1 + (1-lambda) * bi2;
}

  fftw_complex *in_cplx, *out_cplx;
  fftw_plan p;


/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client does nothing more than copy data from its input
 * port to its output port. It will exit when stopped by 
 * the user (e.g. using Ctrl-C on a unix-ish operating system)
 */
int
process (jack_nframes_t nframes, void *arg)
{
  jack_default_audio_sample_t *in, *out;


  int i;
  in = jack_port_get_buffer (input_port, nframes);
  out = jack_port_get_buffer (output_port, nframes);
  memcpy (out, in,
          sizeof (jack_default_audio_sample_t) * nframes);
        
  for (i = 0; i < nframes; ++i){
    /* Hamming and log*/
    in_cplx[i][0] = log((0.54-0.46*sin(2*M_PI*in[i]/(nframes-1)))+1);
    in_cplx[i][1] = 0.0;
  }

  fftw_execute(p); /* repeat as needed */

  double acc = 0;
  int log_idx = 0;
  double f = 0;
  int z = 0;

  gg_set_frame_color(frame, 0, 0, 0);

  for (i = 0; i < 512; ++i){
    double val = out_cplx[i][0]*out_cplx[i][0] + out_cplx[i][1]*out_cplx[i][1];
    acc += val;

    f = i/512.0*22.05e3;

    /* time to add up */
    if (f > z_to_f(z)) {
      acc = 100*sqrt(acc);
      
      printf("\e[%dm%1d\e[0m", (int)(acc+30), 0);

      int col = 0;
      col = 4*atan(acc/10.0);
      if (col > 4) col = 4;
      
      int j;
      for (j = 0; j < 4; ++j) {
        if (j < col) {
          int r, g, b;
          /* interp_color(0, 255, 0, */
          /*              255, 0, 0, */
          /*              &r, &g, &b, */
          /*              atan(acc/10.0)); */
          switch (j) {
          case 0:
          case 1:
            r = 0;
            g = 255;
            b = 0;
            break;
          case 2:
            r = 127;
            g = 127;
            b = 0;
            break;
          case 3:
            r = 255;
            g = 0;
            b = 0;
            break;
          }
          gg_set_pixel_color(frame, z, 3-j, r, g, b);
        } else {
          gg_set_pixel_color(frame, z, 3-j, 0, 0, 0);
        }
        /* gg_set_pixel_color(frame, z, j, acc*3, acc*3, acc*3); */
      }

      ++z;
      acc = 0;
    }
  }

  gg_send_frame((gg_socket *)arg, frame);
  printf("\n");

  return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
  fftw_destroy_plan(p);
  fftw_free(in_cplx); fftw_free(out_cplx);

  exit (1);
}

int
main (int argc, char *argv[])
{
  const char **ports;
  const char *client_name = "simple";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;
  gg_socket *gg_socket;

  /* Open connection to ggg */
  frame = gg_init_frame(COLS, ROWS, 3);
  gg_socket = gg_init_socket("localhost", 0xabac);
  gg_set_duration(gg_socket, FRAME_DURATION);

  /* open a client connection to the JACK server */
  client = jack_client_open (client_name, options, &status, server_name);
  if (client == NULL) {
    fprintf (stderr, "jack_client_open() failed, "
             "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf (stderr, "Unable to connect to JACK server\n");
    }
    exit (1);
  }
  if (status & JackServerStarted) {
    fprintf (stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf (stderr, "unique name `%s' assigned\n", client_name);
  }

  /* tell the JACK server to call `process()' whenever
     there is work to be done.
  */

  jack_set_process_callback (client, process, gg_socket);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us.
  */

  jack_on_shutdown (client, jack_shutdown, 0);

  /* display the current sample rate. 
   */

  printf ("engine sample rate: %" PRIu32 "\n",
          jack_get_sample_rate (client));

  /* create two ports */

  input_port = jack_port_register (client, "input",
                                   JACK_DEFAULT_AUDIO_TYPE,
                                   JackPortIsInput, 0);
  output_port = jack_port_register (client, "output",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput, 0);

  if ((input_port == NULL) || (output_port == NULL)) {
    fprintf(stderr, "no more JACK ports available\n");
    exit (1);
  }


  /* FFTW */
  in_cplx = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 1024);
  out_cplx = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 1024);

  p = fftw_plan_dft_1d(1024, in_cplx, out_cplx, FFTW_FORWARD, FFTW_ESTIMATE);

  /* Tell the JACK server that we are ready to roll.  Our
   * process() callback will start running now. */

  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    exit (1);
  }

  /* Connect the ports.  You can't do this before the client is
   * activated, because we can't make connections to clients
   * that aren't running.  Note the confusing (but necessary)
   * orientation of the driver backend ports: playback ports are
   * "input" to the backend, and capture ports are "output" from
   * it.
   */

  ports = jack_get_ports (client, NULL, NULL,
                          JackPortIsPhysical|JackPortIsOutput);
  if (ports == NULL) {
    fprintf(stderr, "no physical capture ports\n");
    exit (1);
  }

  if (jack_connect (client, ports[0], jack_port_name (input_port))) {
    fprintf (stderr, "cannot connect input ports\n");
  }

  free (ports);
	
  ports = jack_get_ports (client, NULL, NULL,
                          JackPortIsPhysical|JackPortIsInput);
  if (ports == NULL) {
    fprintf(stderr, "no physical playback ports\n");
    exit (1);
  }

  if (jack_connect (client, jack_port_name (output_port), ports[0])) {
    fprintf (stderr, "cannot connect output ports\n");
  }

  free (ports);

  /* keep running until stopped by the user */

  sleep (-1);

  /* this is never reached but if the program
     had some other way to exit besides being killed,
     they would be important to call.
  */

  jack_client_close (client);
  exit (0);
}
