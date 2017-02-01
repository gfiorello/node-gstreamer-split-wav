#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <fstream>
#include <string>
#include <glib/gstdio.h>
#include <gst/gst.h>

#undef NDEBUG

namespace gstSplitWav {
  
  void exit_if_true(gboolean condition, const gchar * message) {
    if (condition) {
      g_printerr("FATAL: %s\n", message);
      exit(2);
    }
  }

  GstElement* my_gst_element_factory_make(const gchar * factoryname, const gchar * name) {
    GstElement *ret = gst_element_factory_make(factoryname, name);
    if (!ret) {
      g_printerr("FATAL: Couldn't create element %s/%s\n", factoryname, name);
      exit(2);
    }
    return ret;
  }

  GstPad* my_gst_element_get_static_pad(GstElement * element, const gchar * name) {
    GstPad *ret = gst_element_get_static_pad(element, name);
    if (!ret) {
      g_printerr("FATAL: Couldn't get %s pad for element\n", name);
      exit(2);
    }
    return ret;
  }

  GstPad* my_gst_element_get_request_pad(GstElement * element, const gchar * name) {
    GstPad *ret = gst_element_get_request_pad(element, name);
    if (!ret) {
      g_printerr("FATAL: Couldn't get %s pad for element\n", name);
      exit(2);
    }
    return ret;
  }

  GstElement* my_gst_bin_new(const gchar * name) {
    GstElement *ret = gst_bin_new(name);
    if (!ret) {
      g_printerr("FATAL: Couldn't create bin %s\n", name);
      exit(2);
    }
    return ret;
  }

  GstElement* my_gst_pipeline_new(const gchar * name) {
    GstElement *ret = gst_pipeline_new(name);
    if (!ret) {
      g_printerr("FATAL: Couldn't create pipeline %s\n", name);
      exit(2);
    }
    return ret;
  }

  std::string get_file_contents(const char *filename) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
      std::string contents;
      in.seekg(0, std::ios::end);
      contents.resize(in.tellg());
      in.seekg(0, std::ios::beg);
      in.read(&contents[0], contents.size());
      in.close();
      return(contents);
    }
    return "";
  }

} //namespace gstSplitWav

#endif