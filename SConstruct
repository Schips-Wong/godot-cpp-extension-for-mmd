#!/usr/bin/env python
import os
import sys

from methods import print_error


libname = "bullet-mmd"
projectdir = "project"

localEnv = Environment(tools=["default"], PLATFORM="")

# Build profiles can be used to decrease compile times.
# You can either specify "disabled_classes", OR
# explicitly specify "enabled_classes" which disables all other classes.
# Modify the example file as needed and uncomment the line below or
# manually specify the build_profile parameter when running SCons.

# localEnv["build_profile"] = "build_profile.json"

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"api_version": 4.3, "env": env, "customs": customs})

# Extension specific flags live in a clone, so godot-cpp keeps its own build signatures
# (mutating the shared env would force a full rebuild of the bindings on every change).
build_env = env.Clone()
build_env.Append(CPPPATH=["src/", "thirdparty/bullet3/src"])
build_env.Append(CPPDEFINES=["_CRT_SECURE_NO_WARNINGS"])

# Bullet must use the same scalar precision as godot-cpp (real_t).
if env["precision"] == "double":
    build_env.Append(CPPDEFINES=["BT_USE_DOUBLE_PRECISION"])

sources = Glob("src/*.cpp") + Glob("src/bullet/*.cpp") + Glob("src/mmd/*.cpp")


def collect_sources(root):
    found = []
    for directory, _subdirs, files in os.walk(root):
        for name in files:
            if name.endswith(".cpp"):
                found.append(os.path.join(directory, name).replace("\\", "/"))
    return sorted(found)


# Bullet 3 is compiled straight into the extension, so there is no external dependency to ship.
# The files are compiled one by one (like Bullet's own CMake build): several of them define
# BT_USE_SSE_IN_API locally, which breaks the amalgamated translation units on MSVC.
bullet_root = "thirdparty/bullet3/src"
for bullet_module in ["BulletCollision", "BulletDynamics", "LinearMath"]:
    sources += [build_env.File(path) for path in collect_sources(os.path.join(bullet_root, bullet_module))]

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# .dev doesn't inhibit compatibility, so we don't need to key it.
# .universal just means "compatible with all relevant arches" so we don't need to key it.
suffix = env['suffix'].replace(".dev", "").replace(".universal", "")

lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

library = build_env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=sources,
)

copy = env.Install("{}/bin/{}/".format(projectdir, env["platform"]), library)

default_args = [library, copy]
Default(*default_args)
