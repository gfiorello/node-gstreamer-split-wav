#include "common.h"
#include "gstsplitwav.h"

namespace gstSplitWav {
	
	using Nan::ThrowError;
	using Nan::Persistent;
	using Nan::HandleScope;
	using Nan::EscapableHandleScope;
	using Nan::ObjectWrap;
	using Nan::GetCurrentContext;
	using Nan::True;
	using Nan::Undefined;
	using Nan::SetPrototypeMethod;

	using v8::Function;
	using v8::FunctionTemplate;
	using v8::Local;
	using v8::Object;
	using v8::String;
	using v8::Number;
	using v8::Value;
	using v8::Boolean;
	
	void GstSplitWav::Init(Local<Object> exports) {
		Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
		tpl->SetClassName(Nan::New<String>("splitWav").ToLocalChecked());
	 	tpl->InstanceTemplate()->SetInternalFieldCount(1);
		// Prototype
		SetPrototypeMethod(tpl, "run", Run);
		SetPrototypeMethod(tpl, "pollBus", PollBus);

		Persistent<Function> constructorPersist;
	 	constructorPersist.Reset(tpl->GetFunction());

		Local<Function> constructor = Nan::New(constructorPersist);
	 	exports->Set(Nan::New<String>("splitWav").ToLocalChecked(), constructor);
  	}

	GstSplitWav::GstSplitWav(const char *inputFile, const char *outputDir, bool keepTimestamp, bool keepFiles) {
		/*	Init pipeline */
		_pipeline = my_gst_pipeline_new("save-sec");
		_queue = my_gst_element_factory_make("queue", "queue");
		/*	Get the starting time in microseconds */
		_birthdate = g_get_real_time() / SECOND_IN_MSECS;

		GstElement *filesrc = my_gst_element_factory_make("filesrc", "filesrc");
		GstElement *wavparse = my_gst_element_factory_make("wavparse", "wavparse");
		GstElement *identity = my_gst_element_factory_make("identity", "identity");
		GstElement *audioconvert = my_gst_element_factory_make("audioconvert", "audioconvert");
		GstElement *audioresample = my_gst_element_factory_make("audioresample", "audioresample");
		GstElement *vorbisenc = my_gst_element_factory_make("vorbisenc", "vorbisenc");
		
		_secnum = 0;
		_numFrames = 0;
		_probeId = 0;
		_isLastFile	= FALSE;
		_ignoreEOS = FALSE;
		_keepStamps = keepTimestamp;
		_keepFiles = keepFiles;
		_fileDir = g_strdup_printf("%s", outputDir);

		if (_keepStamps) {
			_storedate = _birthdate;
		}
		_filename = g_strdup_printf("%" G_GINT64_FORMAT "_%05lu", _birthdate, _secnum);
		
		#ifdef NDEBUG
		g_print("Start time:\t\t%" G_GINT64_FORMAT "\n", _birthdate);
		g_print("Target folder:\t\t%s\n", _fileDir);
		g_print("Source file:\t\t%s\n", inputFile);
		g_print("Target file:\t\t%s\n", _filename);
		#endif
		
		_savebin = newSaveBin();
		exit_if_true(!_savebin, "Couldn't create savebin");
       
		g_object_set(G_OBJECT(filesrc), "location", inputFile, NULL);
		g_object_set(G_OBJECT(filesrc), "do-timestamp", TRUE, NULL);
		g_object_set(G_OBJECT(identity), "sync", TRUE, NULL);
		
		gst_bin_add_many(GST_BIN(_pipeline),
			filesrc, wavparse, identity, audioconvert, audioresample, vorbisenc,
			_queue, _savebin,
			NULL);

		gst_element_link_many(filesrc, wavparse, identity, audioconvert, audioresample,
			vorbisenc, _queue, _savebin,
			NULL);
			
		resetProbe();
	}

	GstSplitWav::~GstSplitWav() {
		#ifdef NDEBUG
		g_print("processing is dying\n");
		#endif
	}

	void GstSplitWav::run() {
		gst_element_set_state(_pipeline, GST_STATE_PLAYING);
		#ifdef NDEBUG
		g_print("Running...\n");
		#endif
		
		#ifdef NDEBUG
		// g_print("Setting pipeline to NULL state...\n");
		#endif	
		// gst_element_set_state(pipeline, GST_STATE_NULL);
		// gst_object_unref(GST_OBJECT(pipeline));
	}
	
	void GstSplitWav::New(const FunctionCallbackInfo<Value>& args) {
		EscapableHandleScope scope;
    	if (args.Length() < 2) {
      	ThrowError("Wrong number of arguments: inputFile and outputDir are required.");
      	scope.Escape(Undefined());
      	return;
    	}

    	String::Utf8Value inputFile(args[0]->ToString());
    	String::Utf8Value outputDir(args[1]->ToString());
		bool keepTimeStamp = args[2]->BooleanValue();
		bool keepFiles = args[3]->BooleanValue();
		
		GstSplitWav *obj = new GstSplitWav(*inputFile, *outputDir, keepTimeStamp, keepFiles);
    	obj->Wrap(args.This());
    	scope.Escape(args.This());
	}

	void GstSplitWav::Run(const FunctionCallbackInfo<Value>& args) {
		EscapableHandleScope scope;
		GstSplitWav *obj = ObjectWrap::Unwrap<GstSplitWav>(args.This());
		obj->run();
    	scope.Escape(True());
  	}
	
	void GstSplitWav::PollBus(const FunctionCallbackInfo<Value>& args) {
		EscapableHandleScope scope;
		GstSplitWav* obj = ObjectWrap::Unwrap<GstSplitWav>(args.This());

		if (args.Length() == 0 || !args[0]->IsFunction()) {
			ThrowError("Callback is required and must be a Function.");
      	scope.Escape(Undefined());
      	return;
		}

		Local<Function> callback = Local<Function>::Cast(args[0]);

		BusRequest *br = new BusRequest();
		br->request.data = br;
		br->callback.Reset(callback);
		br->obj = obj;
		obj->Ref();

		uv_queue_work(uv_default_loop(), &br->request, _doPollBus, _polledBus);
		scope.Escape(Undefined());
	}

	GstElement* GstSplitWav::newSaveBin() {
		gchar *filePath;
		if (_fileDir) {
			filePath = g_strdup_printf("%s%s.tmp", _fileDir, _filename);
		} else {
			filePath = (gchar*)"/dev/null";
		}
		
		GstElement *bin = my_gst_bin_new("savebin");
		GstElement *mux = my_gst_element_factory_make("oggmux", "oggmux");
		GstElement *filesink	= my_gst_element_factory_make("filesink", "filesink");

		gst_bin_add_many(GST_BIN(bin), mux, filesink, NULL);
		gst_element_link_many(mux, filesink, NULL);
		
		#ifdef NDEBUG
		g_print("(new_save_bin) writing to %s\n", filePath);
		#endif
		g_object_set(G_OBJECT(filesink), "location", filePath, NULL);

		GstPad *pad = my_gst_element_get_request_pad(mux, "audio_%u");
		gst_element_add_pad(bin, gst_ghost_pad_new("audio_sink", pad));
		gst_object_unref(GST_OBJECT(pad));
		return bin;
	}

	void GstSplitWav::_doPollBus(uv_work_t *req) {
		BusRequest *br = static_cast<BusRequest*>(req->data);
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(br->obj->_pipeline));
		if (!bus) return;
		br->msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
	}

	void GstSplitWav::_polledBus(uv_work_t *req, int n) {
		BusRequest *br = static_cast<BusRequest*>(req->data);
		if (br->msg) {
			switch (GST_MESSAGE_TYPE(br->msg)) {
				case GST_MESSAGE_EOS:
					if (br->obj->_ignoreEOS) {
						br->obj->_ignoreEOS = FALSE;
						#ifdef NDEBUG
						g_print("Changing file\n");
						#endif
						br->obj->changeFile(br);
						#ifdef NDEBUG
						g_print("Unblocking the pad\n");
						#endif
						br->obj->resetProbe();
					} else {
						g_print("End of stream\n");
						#ifdef NDEBUG
						g_print("Closing last file\n");
						#endif
						br->obj->_isLastFile = TRUE;
						br->obj->changeFile(br);
					}
					break;
				case GST_MESSAGE_ERROR: {
					GError *error = NULL;
					gst_message_parse_error(br->msg, &error, NULL);
					g_printerr("Error: %s\n", error->message);
					g_error_free(error);
					break;
				}
				default:
				break;
			}
			gst_message_unref(br->msg);
		}
		uv_queue_work(uv_default_loop(), &br->request, _doPollBus, _polledBus);
	}
	
	void GstSplitWav::changeFile(BusRequest *br) {
		HandleScope scope;
		Local<Object> m = Nan::New<Object>();
		gchar	*source = g_strdup_printf("%s/%s.tmp", _fileDir, _filename);
		
		/*	Pause pipeline to unlink and change savebin */
		gst_element_set_state(_savebin, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(_pipeline), _savebin);

		std::string fileContent = get_file_contents(source);
		
		m->Set(
			Nan::New<String>("filename").ToLocalChecked(),
			Nan::New<String>(_filename).ToLocalChecked()
      );

		m->Set(
			Nan::New<String>("content").ToLocalChecked(),
			Nan::CopyBuffer((char*)fileContent.data(), fileContent.size()).ToLocalChecked()
		);

		m->Set(
			Nan::New<String>("isLastFile").ToLocalChecked(),
			Nan::New<Boolean>(br->obj->_isLastFile)
		);
		
		if (_keepFiles) {
			gchar *output = g_strdup_printf("%s%s.ogg", _fileDir, _filename);
			g_rename(source, output);
		} else {
			/*	Delete consumed file */
			g_remove(source);
			#ifdef NDEBUG
			g_print("Removed %s\n", source);
			#endif
		}
		
		Local<Value> argv[1] = { m };
		Local<Function> cb = Nan::New(br->callback);
		cb->Call(GetCurrentContext()->Global(), 1, argv);
		
		/*	if is not the EOF */
		if (!_isLastFile) {
			_secnum++;
			_birthdate = g_get_real_time() / SECOND_IN_MSECS;
			_filename = g_strdup_printf("%" G_GINT64_FORMAT "_%05lu", (_keepStamps) ? _storedate : _birthdate, _secnum);
			
			_savebin = newSaveBin();
			gst_bin_add(GST_BIN(_pipeline), _savebin);
			gst_element_link(_queue, _savebin);
			gst_element_set_state(_savebin, GST_STATE_PLAYING);
		}
	}

	void GstSplitWav::resetProbe() {
		GstPad *pad	= gst_element_get_static_pad(_queue, "src");
		gulong newProbeId = gst_pad_add_probe(pad, 
			(GstPadProbeType)(GST_PAD_PROBE_TYPE_BUFFER|GST_PAD_PROBE_TYPE_BLOCK), 
			(GstPadProbeCallback)GstSplitWav::probeData, this, NULL);
		
		exit_if_true(!newProbeId, "Couldn't set the probe on queue src");
		if (_probeId) {
			gst_pad_remove_probe(pad, _probeId);
		}
		_probeId = newProbeId;
		gst_object_unref(pad);
	}

	GstPadProbeReturn GstSplitWav::probeData(GstPad *pad, GstPadProbeInfo *info, gpointer data) {
		GstSplitWav *self = reinterpret_cast<GstSplitWav*>(data);
		gint64 elapsed_time = g_get_real_time() / SECOND_IN_MSECS;
		
		(self->_numFrames)++;
		elapsed_time -= self->_birthdate;
		
		#ifdef NDEBUG
		g_print("Elapsed time: %lu - ", elapsed_time);
		g_print("buffer #%d\n", self->_numFrames);
		#endif

		GstBuffer *buffer	= GST_PAD_PROBE_INFO_BUFFER(info);
		buffer = gst_buffer_make_writable(buffer);
		GstBufferFlags flags	= (GstBufferFlags)GST_BUFFER_FLAGS(buffer);
		GST_PAD_PROBE_INFO_DATA(info) = buffer;
		
		if (!flags) {
			if (elapsed_time >= SECOND_IN_MSECS) {
				self->_numFrames = 0;
				GstPad *savebinpad = my_gst_element_get_static_pad(self->_savebin, "audio_sink");
				self->_ignoreEOS = TRUE;
				gst_pad_send_event(savebinpad, gst_event_new_eos());
				gst_object_unref(GST_OBJECT(savebinpad));
				return GST_PAD_PROBE_OK;
			}
		}
		return GST_PAD_PROBE_PASS;
	}
	
  	void init(Local<Object> exports) {
	 	gst_init(NULL, NULL);
	 	GstSplitWav::Init(exports);
  	}

  	NODE_MODULE(gstSplitWav, init);

} // namespace gstSplitWav