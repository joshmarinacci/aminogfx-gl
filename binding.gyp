{
    "targets": [
        {
            "target_name": "aminonative",
            "sources":[
                "src/base.cpp",
                "src/base_js.cpp",
                "src/base_weak.cpp",

                "src/fonts/vector.c",
                "src/fonts/vertex-buffer.c",
                "src/fonts/vertex-attribute.c",
                "src/fonts/texture-atlas.c",
                "src/fonts/texture-font.c",
                "src/fonts/utf8-utils.c",
                "src/fonts/distance-field.c",
                "src/fonts/edtaa3func.c",
                "src/fonts/shader.c",
                "src/fonts/mat4.c",
                "src/fonts.cpp",

                "src/images/nanojpeg.c",
                "src/images/upng.c",
                "src/images.cpp",

                "src/shaders.cpp",
                "src/renderer.cpp",
                "src/mathutils.cpp"
            ],
            "include_dirs": [
                "<!(node -e \"require('nan')\")",
                "src/",
                "src/fonts/",
                "src/images/"
            ],
            "cflags": [
                "-Wall",
                "-std=c++11",
                # tried to get stack traces on RPi but does not work
                #"-O0",
                #"-g",
                "-funwind-tables"
            ],

            'conditions': [
                ['OS=="mac"', {
                    "include_dirs": [
                        " <!@(freetype-config --cflags)",
                        " <!@(pkg-config --cflags glfw3)",
                    ],
                    "libraries": [
                        " <!@(pkg-config --libs glfw3)",
                        '-framework OpenGL',
                        '-framework OpenCL',
                        '-framework IOKit',
                        '<!@(freetype-config --libs)'
                    ],
                    "sources": [
                        "src/mac.cpp",
                    ],
                    "defines": [
                        "MAC",
                        "GLFW_NO_GLU",
                        "GLFW_INCLUDE_GL3",

                        # VAO not working
                        #"FREETYPE_GL_USE_VAO"
                    ],
                    "xcode_settings": {
                        "OTHER_CPLUSPLUSFLAGS": [
                            "-std=c++11",
                            "-stdlib=libc++"
                        ],
                        "OTHER_LDFLAGS": [
                            "-stdlib=libc++"
                        ],
                        "MACOSX_DEPLOYMENT_TARGET": "10.7"
                    }
                }],

                ['OS=="linux"', {
					"conditions" : [
	                    ["target_arch=='arm'", {
		                    "sources": [
		                        "src/rpi.cpp"
		                    ],
		                    "libraries": [
		                        "-L/opt/vc/lib/ -lbcm_host",
		                        "-lGLESv2",
		                        "-lEGL",
		                        '<!@(freetype-config --libs)',
		                    ],
		                    "defines": [
		                        "RPI"
		                    ],
		                    "include_dirs": [
		                        "/opt/vc/include/",
		                        "/usr/include/freetype2",
		                        "/opt/vc/include/interface/vcos/pthreads",
		                        "/opt/vc/include/interface/vmcs_host/linux",
		                        '<!@(freetype-config --cflags)'
		                    ]
		                }],

		                ["target_arch!='arm'", {
		                    "sources": [
		                        "src/mac.cpp"
		                    ],
		                    "libraries":[
		                        '<!@(freetype-config --libs)',
		                        "-lglfw",
		                    ],
		                    "defines": [
		                        "GL_GLEXT_PROTOTYPES",
		                        "LINUX"
		                    ],
		                    "include_dirs": [
		                        "/usr/include/freetype2",
		                        "<!@(freetype-config --cflags)"
		                    ]
		                }]
		            ]
                }]
            ]
        },
        {
            "target_name": "action_after_build",
            "type": "none",
            "dependencies": [ "<(module_name)" ],
            "copies": [{
                "files": [ "<(PRODUCT_DIR)/<(module_name).node" ],
                "destination": "<(module_path)"
            }]
        }
    ]
}
