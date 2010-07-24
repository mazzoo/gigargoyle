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
#define FRAME_DURATION 1.0f/30*1e6

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
gg_frame *frame;
gg_socket *s;

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

  fftw_complex *in_cplx, *out_cplx;
  fftw_plan p;

  int i;
	
  in = jack_port_get_buffer (input_port, nframes);
  out = jack_port_get_buffer (output_port, nframes);
  memcpy (out, in,
          sizeof (jack_default_audio_sample_t) * nframes);
        
  in_cplx = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * nframes);
  out_cplx = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * nframes);

  for (i = 0; i < nframes; ++i){
    /* Hamming and log*/
    in_cplx[i][0] = log((0.54-0.46*sin(2*M_PI*in[i]/(nframes-1)))+1);
    in_cplx[i][1] = 0.0;
  }

  p = fftw_plan_dft_1d(nframes, in_cplx, out_cplx, FFTW_FORWARD, FFTW_ESTIMATE);
  fftw_execute(p); /* repeat as needed */

  double acc = 0;
  int log_idx = 0;

  /* gg_set_frame_color(frame, 0, 0, 0); */

  for (i = 0; i < 40; ++i){
    /* log f scale */
    log_idx = exp(i/12.0);
    if (log_idx > nframes/2) {
      log_idx = nframes-1;
    };
          
    double val = sqrt(out_cplx[i][0]*out_cplx[i][0] + out_cplx[i][1]*out_cplx[i][1]);
    acc += val;
          
    if (i > log_idx) {
      int j;
      int col = acc*100;
      if (col > 4) col = 4;

      printf("\e[%dm%02d\e[0m", (int)(acc*100)+30, (int)col);
      for (j = 0; j < 4; ++j) {
        if (j < col) {
          gg_set_pixel_color(frame, i, 3-j, 255, 255, 255);
        } else {
          gg_set_pixel_color(frame, i, 3-j, 0, 0, 0);
        }
      }
      
      acc = 0;
    }
  }
  if (i > log_idx) {
    gg_send_frame(s, frame);
    printf("\n");
  }

  fftw_destroy_plan(p);
  fftw_free(in_cplx); fftw_free(out_cplx);

  return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
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
	
  /* Open connection to ggg */
  frame = gg_init_frame(COLS, ROWS, 3);
  s = gg_init_socket("localhost", 0xabac);
  gg_set_duration(s, FRAME_DURATION);

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

  jack_set_process_callback (client, process, 0);

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
