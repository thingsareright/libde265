/*
 * H.265 video codec.
 * Copyright (c) 2013-2014 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of libde265.
 *
 * libde265 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libde265 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libde265.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libde265/en265.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libde265/configparam.h"
#include "libde265/image-io.h"
#include "libde265/encoder/encoder-core.h"
#include "libde265/util.h"
#include "image-io-png.h"

#if DE265_ENABLE_X11_SCREEN_GRABBING
#  include "image-io-x11grab.h"
#endif

#include <getopt.h>



#if HAVE_VIDEOGFX
#include <libvideogfx.hh>
using namespace videogfx;

void debug_show_image_libvideogfx(const image* input, int slot)
{
  static X11Win debugwin;
  static bool opened=false;
  int w = input->get_width();
  int h = input->get_height();
  if (!opened) {
    opened=true;
    debugwin.Create(w,h, "debug");
  }


  bool yuv420 = false;

  Image<Pixel> img;

  if (yuv420) {
    img.Create(w,h,Colorspace_YUV, Chroma_420);

    for (int y=0;y<h;y++)
      memcpy(img.AskFrameY()[y], input->get_image_plane_at_pos<uint8_t>(0,0,y), w);

    for (int y=0;y<h/2;y++) {
      memcpy(img.AskFrameU()[y], input->get_image_plane_at_pos<uint8_t>(1,0,y), w/2);
      memcpy(img.AskFrameV()[y], input->get_image_plane_at_pos<uint8_t>(2,0,y), w/2);
    }
  }
  else {
    img.Create(w,h,Colorspace_RGB);

    for (int y=0;y<h;y++) {
      memcpy(img.AskFrameR()[y], input->get_image_plane_at_pos<uint8_t>(2,0,y), w);
      memcpy(img.AskFrameG()[y], input->get_image_plane_at_pos<uint8_t>(0,0,y), w);
      memcpy(img.AskFrameB()[y], input->get_image_plane_at_pos<uint8_t>(1,0,y), w);
    }
  }

  debugwin.Display(img);
  //debugwin.WaitForKeypress();
}
#endif



int show_help=false;
int verbosity=0;

static struct option long_options[] = {
  {"help",       no_argument,       &show_help, 1 },
  {"verbose",    no_argument,       0, 'v' },
  {0,            0,                 0,  0 }
};


struct inout_params
{
  inout_params();

  // input

  option_int first_frame;
  option_int max_number_of_frames;

  option_string input_yuv;
  option_int input_width;
  option_int input_height;

  option_bool input_is_rgb;
  option_bool input_is_screengrabbing;

  // output

  option_string output_filename;

  // debug

  option_string reconstruction_yuv;


  void register_params(config_parameters& config);
};


inout_params::inout_params()
{
  input_yuv.set_ID("input"); input_yuv.set_short_option('i');
  input_yuv.set_default("paris_cif.yuv");

  output_filename.set_ID("output"); output_filename.set_short_option('o');
  output_filename.set_default("out.bin");

  reconstruction_yuv.set_ID("input");
  reconstruction_yuv.set_default("recon.yuv");

  first_frame.set_ID("first-frame");
  first_frame.set_default(0);
  first_frame.set_minimum(0);

  max_number_of_frames.set_ID("frames");
  max_number_of_frames.set_short_option('f');
  max_number_of_frames.set_minimum(1);
  //max_number_of_frames.set_default(INT_MAX);

  input_width.set_ID("width"); input_width.set_short_option('w');
  input_width.set_minimum(1);  input_width.set_default(352);

  input_height.set_ID("height"); input_height.set_short_option('h');
  input_height.set_minimum(1); input_height.set_default(288);

  input_is_rgb.set_ID("rgb");
  input_is_rgb.set_default(false);
  input_is_rgb.set_description("input is sequence of RGB PNG images");

  input_is_screengrabbing.set_ID("screen");
  input_is_screengrabbing.set_default(false);
  input_is_screengrabbing.set_description("input is real-time screen-grabbing");
}


void inout_params::register_params(config_parameters& config)
{
  config.add_option(&input_yuv);
  config.add_option(&output_filename);
  config.add_option(&first_frame);
  config.add_option(&max_number_of_frames);
  config.add_option(&input_width);
  config.add_option(&input_height);
#if HAVE_VIDEOGFX
  if (videogfx::PNG_Supported()) {
    config.add_option(&input_is_rgb);
  }
#endif
#if DE265_ENABLE_X11_SCREEN_GRABBING
  config.add_option(&input_is_screengrabbing);
#endif
}


void test_parameters_API(en265_encoder_context* ectx)
{
  const char** param = en265_list_parameters(ectx);
  if (param) {
    for (int i=0; param[i]; i++) {
      printf("|%s| ",param[i]);

      enum en265_parameter_type type = en265_get_parameter_type(ectx, param[i]);
      const char* type_name="unknown";
      switch (type) {
      case en265_parameter_int: type_name="int"; break;
      case en265_parameter_bool: type_name="bool"; break;
      case en265_parameter_string: type_name="string"; break;
      case en265_parameter_choice: type_name="choice"; break;
      }

      printf("(%s)",type_name);

      if (type==en265_parameter_choice) {
        const char** choices = en265_list_parameter_choices(ectx, param[i]);
        if (choices) {
          for (int k=0; choices[k]; k++) {
            printf(" %s",choices[k]);
          }
        }
      }

      printf("\n");
    }
  }

  // en265_set_parameter_int(ectx, "min-tb-size", 8);
}


extern int skipTBSplit, noskipTBSplit;
extern int zeroBlockCorrelation[6][2][5];

/*LIBDE265_API*/ ImageSink_YUV reconstruction_sink;


int main(int argc, char** argv)
{
  de265_init();

  en265_encoder_context* ectx = en265_new_encoder();


  bool cmdline_errors = false;

  // --- in/out parameters ---

  struct inout_params inout_params;
  config_parameters inout_param_config;
  inout_params.register_params(inout_param_config);

  int first_idx=1;
  if (!inout_param_config.parse_command_line_params(&argc,argv, &first_idx, true)) {
    cmdline_errors = true;
  }


  // --- read encoder parameters ---

  if (en265_parse_command_line_parameters(ectx, &argc, argv) != DE265_OK) {
    cmdline_errors = true;
  }



  while (1) {
    int option_index = 0;

    int c = getopt_long(argc, argv, "v"
                        , long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'v': verbosity++; break;
    }
  }


  // --- show usage information ---

  if (optind != argc || cmdline_errors || show_help) {
    fprintf(stderr," enc265  v%s\n", de265_get_version());
    fprintf(stderr,"--------------\n");
    fprintf(stderr,"usage: enc265 [options]\n");
    fprintf(stderr,"The video file must be a raw YUV file or a PNG sequence for RGB input\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"options:\n");
    fprintf(stderr,"      --help         show help\n");
    fprintf(stderr,"  -v, --verbose      increase verbosity level (up to 3 times)\n");

    inout_param_config.print_params();
    fprintf(stderr,"\n");
    en265_show_parameters(ectx);

    exit(show_help ? 0 : 5);
  }



  de265_set_verbosity(verbosity);

#if HAVE_VIDEOGFX
  //debug_set_image_output(debug_show_image_libvideogfx);
#endif

  //test_parameters_API(ectx);


  if (strlen(inout_params.reconstruction_yuv.get().c_str()) != 0) {
    reconstruction_sink.set_filename(inout_params.reconstruction_yuv.get().c_str());
    //ectx.reconstruction_sink = &reconstruction_sink;
  }

  acceleration_functions accel;
  accel.init( de265_get_CPU_capabilites_all_autodetected(),
              0 );


  ImageSource* image_source;
  ImageSource_YUV image_source_yuv;

#if HAVE_VIDEOGFX
  ImageSource_PNG image_source_png;
#endif

#if DE265_ENABLE_X11_SCREEN_GRABBING
  ImageSource_X11Grab image_source_x11grab;
  image_source_x11grab.setAccelerationFunctions(accel);
#endif

#if DE265_ENABLE_X11_SCREEN_GRABBING
  if (inout_params.input_is_screengrabbing) {
    image_source_x11grab.set_size(0,0);
    image_source_x11grab.set_size(1920,1072);
    image_source = &image_source_x11grab;
  }
  else
#endif
#if HAVE_VIDEOGFX
  if (inout_params.input_is_rgb) {
    image_source_png.set_input_file(inout_params.input_yuv.get().c_str());
    image_source = &image_source_png;
  }
  else
#endif
  {
    image_source_yuv.set_input_file(inout_params.input_yuv.get().c_str(),
                                    inout_params.input_width,
                                    inout_params.input_height);
    image_source = &image_source_yuv;
  }


  PacketSink_File packet_sink;
  packet_sink.set_filename(inout_params.output_filename.get().c_str());


  // --- run encoder ---

  image_source->skip_frames( inout_params.first_frame );

  en265_start_encoder(ectx, 0);

  int maxPoc = INT_MAX;
  if (inout_params.max_number_of_frames.is_defined()) {
    maxPoc = inout_params.max_number_of_frames;
  }


  fps_estimator fpsEstimator;

  bool eof = false;
  for (int poc=0; poc<maxPoc && !eof ;poc++)
    {
      // push one image into the encoder

      image* input_image = image_source->get_image();
      if (input_image==NULL) {
        en265_push_eof(ectx);
        eof=true;
      }
      else {
        fpsEstimator.on_frame_decoded( de265_get_time() );
        //debug_show_image_libvideogfx(input_image, 0);

        if (fpsEstimator.fps_measurement_available() && (poc%100)==0) {
          std::cout << "FPS: " << fpsEstimator.get_fps_measurement() << "\n";
        }

        //en265_push_image(ectx, (de265_image*)input_image);

        delete input_image;
      }
#if 0


      // encode images while more are available

      en265_encode(ectx);


      // write all pending packets

      for (;;) {
        en265_packet* pck = en265_get_packet(ectx,0);
        if (pck==NULL)
          break;

        packet_sink.send_packet(pck->data, pck->length);

        en265_free_packet(ectx,pck);
      }
#endif
    }


  // --- print statistics ---

  en265_print_logging((encoder_context*)ectx, "tb-split", NULL);


  en265_free_encoder(ectx);

  de265_free();

  return 0;
}
