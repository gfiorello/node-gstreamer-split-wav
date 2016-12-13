# node-gstreamer-split-wav
==========================

Use GStreamer to split a live wav file

## How?

```javascript
var gst = require('gstreamer-split-wav');
var pipeline = new gst.splitWav(string "sourceFile", string "outputPath", bool keepTimestamp, bool keepFiles);
pipeline.run();
```

### Polling the GStreamer Pipeline Bus

You can asynchronously handle bus messages using gst.pollBus(callback):

```javascript
pipeline.pollBus( (file) => {
	console.log(file.filename); // String
	console.log(file.content); // Buffer content
});
```
