#ifndef GSTSPLITWAV_H
#define GSTSPLITWAV_H

#include <v8.h>
#include <nan.h>
#include <gst/gst.h>

namespace gstSplitWav {

	using v8::Local;
	using v8::Object;
	using v8::Value;
	using v8::Function;

	using Nan::ObjectWrap;
	using Nan::FunctionCallbackInfo;
	using Nan::Persistent;

	class GstSplitWav : public ObjectWrap {
		public:
			static void Init(Local<Object> exports);

			void run();
			Local<Value> pollBus();
		
		private:
			GstSplitWav(const char *inputFile, const char *outputDir, bool keepTimestamp, bool keepFiles);
			~GstSplitWav();
			
			GstElement *_pipeline;
			GstElement *_queue;
			GstElement *_savebin;
			
			gulong _probeId;
			gint64 _birthdate;
			gint64 _storedate;
			gint64 _secnum;
			guint _numFrames;
			gchar *_fileDir;
			gchar *_filename;
			gboolean _ignoreEOS;
			gboolean _keepStamps;
			gboolean _keepFiles;
			gboolean _isLastFile;
			
			struct BusRequest {
				uv_work_t request;
				Persistent<Function> callback;
				GstSplitWav *obj;
				GstMessage *msg;
			};
			
			void changeFile(BusRequest *br);
			void resetProbe();
			GstElement* newSaveBin();
			
			static GstPadProbeReturn probeData(GstPad *pad, GstPadProbeInfo *info, gpointer data);
			static void New(const FunctionCallbackInfo<Value>& args);
			static void Run(const FunctionCallbackInfo<Value>& args);
			static void PollBus(const FunctionCallbackInfo<Value>& info);
			static void _doPollBus(uv_work_t *req);
			static void _polledBus(uv_work_t *req, int);
	};

}  // namespace gstSplitWav
#endif
