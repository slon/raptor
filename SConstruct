env = Environment()

CPPFLAGS=[
    "-std=c++0x", "-g", "-fPIC", "-O3",
    "-isystem", "raptor/fixinclude",
    "-isystem", "test/gmock",
	"-fno-strict-aliasing",
]

env.Append(CPPPATH=["."])
env.Append(LINKFLAGS=["-g"])
env.Append(CPPFLAGS=["-Wall", "-Werror"] + CPPFLAGS)

gmock = env.Library("libgmock.a", ["test/gmock/gmock-gtest-all.cc"], CPPFLAGS=CPPFLAGS)
gmock_main = env.Object("test/gmock/gmock_main.cc", CPPFLAGS=CPPFLAGS)

raptor = env.Library("libraptor.a",
    Glob("raptor/core/*.cpp") + Glob("raptor/io/*.cpp") +
    Glob("raptor/fiber/*.cpp") + ["raptor/core/context_supp.S"])

env.Program("test/run_ut",
    Glob("test/ut/io/*.cpp") + Glob("test/ut/core/*.cpp") +
    Glob("test/ut/fiber/*.cpp") + gmock_main,
    LIBS=[raptor, gmock, "pthread", "ev"])
