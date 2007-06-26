#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdlib.h>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <vw/FileIO.h>
#include <vw/Image.h>
#include <vw/Stereo.h>
using namespace vw;
using namespace vw::stereo;

#include "stereo.h"
#include "nff_terrain.h"


// Allows FileIO to correctly read/write these pixel types
namespace vw {
  template<> struct PixelFormatID<Vector3>   { static const PixelFormatEnum value = VW_PIXEL_XYZ; };
}

int main( int argc, char *argv[] ) {
  set_debug_level(VerboseDebugMessage+11);

  std::string input_file_name, out_prefix, output_file_type, texture_filename;
  unsigned cache_size, max_triangles;
  float mesh_tolerance;
  unsigned simplemesh_h_step, simplemesh_v_step;
  int debug_level;
  
  po::options_description desc("Options");
  desc.add_options()
    ("help", "Display this help message")
    ("simple-mesh", "Generate simple (non-adaptive) mesh")
    ("simplemesh-h-step", po::value<unsigned>(&simplemesh_h_step)->default_value(10), "Horizontal step size for simple meshing algorithm")
    ("simplemesh-v-step", po::value<unsigned>(&simplemesh_v_step)->default_value(10), "Vertical step size for simple meshing algorithm")
    ("mesh-tolerance", po::value<float>(&mesh_tolerance)->default_value(0.001), "Tolerance for the adaptive meshing algorithm")
    ("max_triangles", po::value<unsigned>(&max_triangles)->default_value(1000000), "Maximum triangles for the adaptive meshing algorithm")
    ("cache", po::value<unsigned>(&cache_size)->default_value(1024), "Cache size, in megabytes")
    ("input-file", po::value<std::string>(&input_file_name), "Explicitly specify the input file")
    ("texture-file", po::value<std::string>(&texture_filename), "Specify texture filename")
    ("grayscale-texture", "Use grayscale image processing when modifying the texture image (for .iv and .vrml files only)")
    ("output-prefix,o", po::value<std::string>(&out_prefix)->default_value("mesh"), "Specify the output prefix")
    ("output-filetype,t", po::value<std::string>(&output_file_type)->default_value("ive"), "Specify the output file")
    ("debug-level,d", po::value<int>(&debug_level)->default_value(vw::DebugMessage-1), "Set the debugging output level. (0-50+)");

  po::positional_options_description p;
  p.add("input-file", 1);
  p.add("texture-file", 1);

  po::variables_map vm;
  po::store( po::command_line_parser( argc, argv ).options(desc).positional(p).run(), vm );
  po::notify( vm );

  // Set the Vision Workbench debug level
  set_debug_level(debug_level);
  Cache::system_cache().resize( cache_size*1024*1024 ); 

  if( vm.count("help") ) {
    std::cout << desc << std::endl;
    return 1;
  }

  if( vm.count("input-file") != 1 || vm.count("texture-file") != 1 ) {
    std::cout << "Error: Must specify exactly one pointcloud file and one texture file!" << std::endl;
    std::cout << desc << std::endl;
    return 1;
  }

  DiskImageView<Vector3> point_image(input_file_name);

  std::cout << "\nGenerating 3D mesh from point cloud:\n";
  Mesh mesh_maker;
  if(vm.count("simple-mesh")) {
    mesh_maker.build_simple_mesh(point_image, simplemesh_h_step, simplemesh_v_step);
  } else {
    mesh_maker.build_adaptive_mesh(point_image, mesh_tolerance, max_triangles);
  }
  
  // New style (open scene graph) 3D models
  if (output_file_type == "ive") 
    mesh_maker.write_osg(out_prefix+".ive", texture_filename);
  
  // Old style (open inventor) 3D models
  else if(output_file_type == "iv" || output_file_type == "vrml") {
    std::string corrected_texture_filename = prefix_from_filename(texture_filename)+".jpg";
    if (suffix_from_filename(texture_filename) != "jpg")
      if (vm.count("grayscale-texture") ) {
        DiskImageView<PixelGray<uint8> > texture(texture_filename);
        write_image(corrected_texture_filename, texture);
      } else { 
        DiskImageView<PixelRGB<uint8> > texture(texture_filename);
        write_image(corrected_texture_filename, texture);
      }

    if(output_file_type == "iv") 
      mesh_maker.write_inventor(out_prefix+".iv", corrected_texture_filename);

    if(output_file_type == "vrml") 
      mesh_maker.write_vrml(out_prefix+".vrml", corrected_texture_filename);

  } else {
    std::cout << "Unsupported 3D file type.\n";
  }

  return 0;
}