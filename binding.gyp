{
    "targets": [{
        "target_name": "gstreamer-split-wav",
        "sources": [
            "source/gstsplitwav.cc",
        ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "conditions" : [
            ["OS=='linux'", {
                "include_dirs": [
                    '<!@(pkg-config gstreamer-1.0 --cflags-only-I | sed s/-I//g)'
                ],
                "libraries": [
                    '<!@(pkg-config gstreamer-1.0 --libs)'
                ]
            }]
        ]
    }]
}