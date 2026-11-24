#
# Pebble SDK build script.
#
# Besides the standard app build, this turns config/index.html (the phone
# settings page) into a JS module so PebbleKit JS can open it as a data: URI,
# which keeps the settings page working with no hosting and no network.
#
import io
import json
import os.path

top = '.'
out = 'build'

CONFIG_HTML = 'config/index.html'
CONFIG_MODULE = 'src/pkjs/generated/config_html.js'


def generate_config_module():
    with io.open(CONFIG_HTML, encoding='utf-8') as f:
        html = f.read()
    content = u'// Generated from %s by wscript - do not edit.\nmodule.exports = %s;\n' % (
        CONFIG_HTML, json.dumps(html))
    if os.path.exists(CONFIG_MODULE):
        with io.open(CONFIG_MODULE, encoding='utf-8') as f:
            if f.read() == content:
                return
    if not os.path.isdir(os.path.dirname(CONFIG_MODULE)):
        os.makedirs(os.path.dirname(CONFIG_MODULE))
    with io.open(CONFIG_MODULE, 'w', encoding='utf-8') as f:
        f.write(content)


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


def build(ctx):
    ctx.load('pebble_sdk')
    generate_config_module()

    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js', 'src/pkjs/**/*.json']),
                   js_entry_file='src/pkjs/index.js')
