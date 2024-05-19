Import('env')
import os
import subprocess
import sys
import json
import shutil

root = os.getcwd()

# check the project structure
data_dir = os.path.join(os.getcwd(), 'data')
if os.path.isdir(data_dir) == False :
    os.mkdir(data_dir)

# load config data
with open(f'{root}/configure.json', 'r') as data:
    conf = json.load(data)

# silently check if all modules are installed
subprocess.call([sys.executable, '-m', 'pip', 'install', 'pillow'],
                    stdout=subprocess.DEVNULL)
# starting with i18n
print(f'\n[\U0001F37A] generating locale data ({conf["ui-lang"]})\n')
r = subprocess.Popen([
        sys.executable,
        f'{root}/scripts/generate_translations.py',
        f'--fonts={conf["ui-font"]}',
        f'{conf["ui-lang"]}'
        ],
    stderr=subprocess.STDOUT
)
o, e = r.communicate()
if r.returncode != 0 :
    print(f"\n[\U0001F383] error #{r.returncode}\n")
    sys.exit()

defs = [('VERSION', conf['version'])]
env.Append(CPPDEFINES=defs)

# continue execution
print(f"\n[\U0001F37A] running build of v.{conf['version']}\n")