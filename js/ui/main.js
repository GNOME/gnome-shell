/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;

const Panel = imports.ui.panel;

let panel = null;

function start() {
    panel = new Panel.Panel();
}
