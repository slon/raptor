env = Environment()

CPPFLAGS=[
    "-std=c++0x", "-g", "-fPIC", "-O3",
    "-isystem", "raptor/fixinclude",
    "-isystem", "test/gmock",
    "-fno-strict-aliasing",
	"-Wall", "-Wextra", "-Werror"
]

if ARGUMENTS.get("tsan", False):
   env['CC'] = env['CXX'] = 'clang++-3.5'
   env.Append(CPPFLAGS=['-fsanitize=thread'])
   env.Append(LINKFLAGS=['-fsanitize=thread'])

if ARGUMENTS.get("asan", False):
   env['CC'] = env['CXX'] = 'clang++-3.5'
   env.Append(CPPFLAGS=['-fsanitize=address'])
   env.Append(LINKFLAGS=['-fsanitize=address'])

env.Append(CPPPATH=["."])
env.Append(LINKFLAGS=["-g", "-O3"])
env.Append(CPPFLAGS=CPPFLAGS)

gmock = env.Library("libgmock.a", ["contrib/gmock/gmock-gtest-all.cc"])
gmock_main = env.Object("contrib/gmock/gmock_main.cc")

raptor = env.Library("libraptor.a",
                     Glob("raptor/core/*.cpp") +
                     Glob("raptor/io/*.cpp") +
                     Glob("raptor/daemon/*.cpp") +
                     Glob("raptor/server/*.cpp") +
                     Glob("raptor/client/*.cpp") +
                     ["raptor/core/context_supp.S"])

kafka = env.Library("libraptor_kafka.a",
                    Glob("raptor/kafka/*.cpp"))

LIBS=[kafka, raptor, "pmetrics", "ev", "pthread", "glog", "gflags", "snappy"]

for main_file in Glob("bin/*.cpp"):
    env.Program(str(main_file)[:-4], main_file, LIBS=LIBS)

env.Program("run_ut",
            Glob("test/ut/io/*.cpp") + Glob("test/ut/daemon/*.cpp") +
            Glob("test/ut/core/*.cpp") + Glob("test/ut/kafka/*.cpp") +
			Glob("test/ut/server/*.cpp") + gmock_main,
            LIBS=LIBS + [gmock])

env.Program("test/it/kafka/run_it",
            Glob("test/it/kafka/*.cpp") + gmock_main,
            LIBS=LIBS + [gmock])
