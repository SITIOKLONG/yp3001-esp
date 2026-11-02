Import("env")


def dedupe_libs(target, source, env):
    libs = env.get("LIBS", [])
    seen = set()
    unique = []
    for lib in libs:
        key = str(lib)
        if key in seen:
            continue
        seen.add(key)
        unique.append(lib)
    env.Replace(LIBS=unique)
    print("Deduped LIBS: {} -> {}".format(len(libs), len(unique)))


env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", dedupe_libs)
