/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Main = imports.ui.main;
const Scripting = imports.ui.scripting;

// This performance script measure the most important (core) performance
// metrics for the shell. By looking at the output metrics of this script
// someone should be able to get an idea of how well the shell is performing
// on a particular system.

let METRIC_DESCRIPTIONS = {
    overviewLatencyFirst: "Time to first frame after triggering overview, first time",
    overviewFramesFirst: "Frames displayed when going to overview, first time",
    overviewLatencySubsequent: "Time to first frame after triggering overview, second time",
    overviewFramesSubsequent: "Frames displayed when going to overview, second time"
};

let METRICS = {
};

function run() {
    Scripting.defineScriptEvent("overviewShowStart", "Starting to show the overview");
    Scripting.defineScriptEvent("overviewShowDone", "Overview finished showing");

    yield Scripting.sleep(1000);
    yield Scripting.waitLeisure();
    for (let i = 0; i < 2; i++) {
        Scripting.scriptEvent('overviewShowStart');
        Main.overview.show();
        yield Scripting.waitLeisure();
        Scripting.scriptEvent('overviewShowDone');
        Main.overview.hide();
        yield Scripting.waitLeisure();
    }
}

let showingOverview = false;
let overviewShowStart;
let overviewFrames;
let overviewLatency;

function script_overviewShowStart(time) {
    showingOverview = true;
    overviewShowStart = time;
    overviewFrames = 0;
}

function script_overviewShowDone(time) {
    showingOverview = false;

    if (!('overviewLatencyFirst' in METRICS)) {
        METRICS.overviewLatencyFirst = overviewLatency;
        METRICS.overviewFramesFirst = overviewFrames;
    } else {
        METRICS.overviewLatencySubsequent = overviewLatency;
        METRICS.overviewFramesSubsequent = overviewFrames;
    }
}

function clutter_stagePaintDone(time) {
    if (showingOverview) {
        if (overviewFrames == 0)
            overviewLatency = time - overviewShowStart;

        overviewFrames++;
    }
}
