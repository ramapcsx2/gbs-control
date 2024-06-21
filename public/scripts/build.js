const path = require('path');
const root = path.resolve('.');
const nodeModulesPath = `${root}/node_modules`;
const dataPath = `${root}/data`;
const webRootPath = `${root}/public`;
const fs = require('fs');
if (!fs.existsSync(dataPath)) {
    fs.mkdirSync(dataPath)
}
const minify = require(`${nodeModulesPath}/@node-minify/core`);
const htmlMinifier = require(`${nodeModulesPath}/@node-minify/html-minifier`);
const uglifyJS = require(`${nodeModulesPath}/@node-minify/uglify-js`);
var html = fs
    .readFileSync(`${webRootPath}/src/index.html.tpl`, 'utf-8');
var js = fs
    .readFileSync(`${webRootPath}/src/index.js`, 'utf-8')
    .replaceAll(/\/\*[\s\S]*?\*\/|([^:]|^)\/\/.*$/gm, '');

//
// A simple script which does i18n of HTML template.
// If a variable data reqired in sentence the following expressions may be used:
//          in HTML: L{TAG_NAME[:VAR1[:VAR2[...]]]}
//          in Typescript: 'L{TAG_NAME}'.format([...])
// mind bracket and quotes type around tags...
// In order to keep things "as is" as possible, let's search and replace a tag both HTML and JS
//
const default_ui_lang = 'en';
const config = require(`${root}/configure.json`);
const package = require(`${root}/package.json`);
const lang = require(`${root}/translation.webui.json`);
String.prototype.format = function() {
    a = this;
    for (k in arguments) {
        a = a.replace("{" + k + "}", arguments[k])
    }
    return a
};
const replaceLangRegex = function(matchObj, source, mask, data) {
    matchObj.forEach((res) => {
        let params = [];
        const r0 = new String(res[1]);
        if(r0 !== 'undefined' && r0.length != 0)
            params = r0.substring(1).split(':');
        source = source.replaceAll(
            mask,
            (params.length != 0) ? data.format(...params) : data.toString()
        );
    });
    return source;
};
for(const l in lang) {
    // get sentense with fallback
    let sents = new String(lang[l][config['ui-lang']]);
    if(sents.length == 0)
        sents = new String(lang[l][default_ui_lang]);
    const tag = l;
    if(tag !== "" && sents.length != 0) {
        const regex = new RegExp(String.raw`L{${tag}(\:.+)?}`, 'gm');
        let f = [...html.matchAll(regex)];
        if(f !== "undefined" && f.length != 0) {
            html = replaceLangRegex(f, html, regex, sents);
        } else {
            // try to find this tag in JS
            f = [...js.matchAll(regex)];
            // console.log(`searching ${tag} in JS...`)
            if(f !== undefined || f.length != 0) {
                js = replaceLangRegex(f, js, regex, sents);
            } else
                console.log(`(!) nothing found for '${tag}', check translation tag names`);
        }
    }
}
// i18n end

const icon1024 = fs
    .readFileSync(`${webRootPath}/assets/icons/icon-1024-maskable.png`)
    .toString('base64')
const webUIFont = fs
    .readFileSync(`${webRootPath}/assets/fonts/${config['ui-web-font']}.woff2`)
    .toString('base64')
const material = fs
    .readFileSync(`${webRootPath}/assets/fonts/material.woff2`)
    .toString('base64')
const favicon = fs
    .readFileSync(`${webRootPath}/assets/icons/gbsc-logo.png`)
    .toString('base64')

const css = fs
    .readFileSync(`${webRootPath}/src/style.css`, 'utf-8')
    .replaceAll('${webUIFontName}', config['ui-web-font'])
    .replaceAll('${webUIFont}', webUIFont)
    .replace('${material}', material)

const manifest = fs
    .readFileSync(`${webRootPath}/src/manifest.json`, 'utf-8')
    .replaceAll(/\$\{icon1024\}/g, `data:image/png;base64,${icon1024}`)

minify({
    compressor: uglifyJS,
    content: js,
    options: {
        warnings: true,
        mangle: true,
        compress: true
    }
}).then((minifiedJS) => {
    minify({
        compressor: htmlMinifier,
        content: html
                .replace('${styles}', css)
                .replace('${js}', minifiedJS)
                .replace('${favicon}', `data:image/png;base64,${favicon}`)
                .replace('${VERSION_FIRMWARE}', config['version'])
                .replace('${VERSION_UI}', package['version'])
                .replace(
                    '${manifest}',
                    `data:application/json;base64,${Buffer.from(manifest).toString('base64')}`
                )
                .replace('${icon1024}', `data:image/png;base64,${icon1024}`)
                .trim(),
        options: {
            removeAttributeQuotes: true
        },
    }).then((minifiedHtml) => {
        fs.writeFileSync(
            'data/webui.html',
            minifiedHtml,
            'utf8'
        );
    });
});
