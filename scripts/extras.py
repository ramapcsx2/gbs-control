Import('env')
import os
import shutil

root = os.getcwd()

# prepare WebUI
def before_buildfs(source, target, env):
    print('\n\U0001F37A: building WebUI\n')
    env.Execute("npm run build")

env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs)

# copy FS image to /builds
# def after_buildfs(source, target, env):
#     path = target[0].get_abspath()
#     shutil.copyfile(path, f'{root}/builds/filesystem.bin')
#     print('\n\U0001F37A: filesystem.bin has been copied into /builds\n')

# env.AddPostAction("$BUILD_DIR/littlefs.bin", after_buildfs)

# prepare for ArduinoIDE
def after_build(source, target, env):
    # path = source[0].get_abspath()
    fino = f"{root}/gbs-control.ino"
    try:
        os.remove(fino)
    except FileNotFoundError:
        pass
    shutil.copyfile(f"{root}/src/main.cpp", fino)
    ## fix include paths
    with open(fino, 'r') as f:
        ino = f.read()
    ino = ino.replace('#include "', '#include "src/')
    with open(fino, 'w') as f:
        f.write(ino)
    print("\n\U0001F37A: gbs-control.ino updated\n")
    # shutil.copyfile(os.path.splitext(path)[0] + ".bin", f'{root}/builds/firmware.bin')
    # print("\U0001F37A: firmware.bin has been copied into /builds\n")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)