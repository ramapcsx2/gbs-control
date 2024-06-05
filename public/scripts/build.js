const fs = require('fs')
const html = fs.readFileSync('./../src/index.html.tpl', 'utf-8')
const js = fs.readFileSync('./../src/index.js', 'utf-8')

const icon1024 = fs
    .readFileSync('./../assets/icons/icon-1024-maskable.png')
    .toString('base64')
const oswald = fs
    .readFileSync('./../assets/fonts/oswald.woff2')
    .toString('base64')
const material = fs
    .readFileSync('./../assets/fonts/material.woff2')
    .toString('base64')
const favicon = fs
    .readFileSync('./../assets/icons/gbsc-logo.png')
    .toString('base64')

const css = fs
    .readFileSync('./../src/style.css', 'utf-8')
    .replace('${oswald}', oswald)
    .replace('${material}', material)

const manifest = fs
    .readFileSync('./../src/manifest.json', 'utf-8')
    .replace(/\$\{icon1024\}/g, `data:image/png;base64,${icon1024}`)

fs.writeFileSync(
    './../../data/webui.html',
    html
        .replace('${styles}', css)
        .replace('${js}', js)
        .replace('${favicon}', `data:image/png;base64,${favicon}`)
        .replace(
            '${manifest}',
            `data:application/json;base64,${Buffer.from(manifest).toString('base64')}`
        )
        .replace('${icon1024}', `data:image/png;base64,${icon1024}`),
    'utf8'
)