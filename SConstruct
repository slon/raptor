env = Environment()

CPPFLAGS=[
    "-std=c++0x", "-g", "-fPIC", "-O3",
    "-isystem", "raptor/fixinclude",
    "-isystem", "test/gmock",
    "-fno-strict-aliasing"
]

env.Append(CPPPATH=["."])
env.Append(LINKFLAGS=["-g", "-O3"])
env.Append(CPPFLAGS=CPPFLAGS)

gmock = env.Library("libgmock.a", ["contrib/gmock/gmock-gtest-all.cc"])
gmock_main = env.Object("contrib/gmock/gmock_main.cc")

raptor = env.Library("libraptor.a",
                     Glob("raptor/core/*.cpp") + Glob("raptor/io/*.cpp") +
                     ["raptor/core/context_supp.S"])

kafka = env.Library("libraptor_kafka.a",
                    Glob("raptor/kafka/*.cpp"))

for main_file in Glob("bin/*.cpp"):
    env.Program(str(main_file)[:-4], main_file, LIBS=[
            kafka, raptor, "boost_program_options", "ev", "pthread"])


env.Program("run_ut",
            Glob("ut/io/*.cpp") + Glob("ut/core/*.cpp") + Glob("ut/kafka/*.cpp") + gmock_main,
            LIBS=[kafka, raptor, gmock, "pthread", "ev"])
