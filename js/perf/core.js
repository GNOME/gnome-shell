/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Main = imports.ui.main;
const Scripting = imports.ui.scripting;

// This performance script measure the most important (core) performance
// metrics for the shell. By looking at the output metrics of this script
// someone should be able to get an idea of how well the shell is performing
// on a particular system.

let METRICS = {
    overviewLatencyFirst:
    { description: "Time to first frame after triggering overview, first time",
      units: "us" },
    overviewFramesFirst:
    { description: "Frames displayed when going to overview, first time",
      units: "frames" },
    overviewLatencySubsequent:
    { description: "Time to first frame after triggering overview, second time",
      units: "us"},
    overviewFramesSubsequent:
    { description: "Frames displayed when going to overview, second time",
      units: "us" },
    usedAfterOverview:
    { description: "Malloc'ed bytes after the overview is shown once",
      units: "B" },
    leakedAfterOverview:
    { description: "Additional malloc'ed bytes the second time the overview is shown",
      units: "B" }
};

function run() {
    Scripting.defineScriptEvent("overviewShowStart", "Starting to show the overview");
    Scripting.defineScriptEvent("overviewShowDone", "Overview finished showing");
    Scripting.defineScriptEvent("afterShowHide", "After a show/hide cycle for the overview");

    yield Scripting.sleep(1000);
    yield Scripting.waitLeisure();
    for (let i = 0; i < 2; i++) {
        Scripting.scriptEvent('overviewShowStart');
        Main.overview.show();
        yield Scripting.waitLeisure();
        Scripting.scriptEvent('overviewShowDone');

        Main.overview.hide();
        yield Scripting.waitLeisure();

        global.gc();
        yield Scripting.sleep(1000);
        Scripting.collectStatistics();
        Scripting.scriptEvent('afterShowHide');
    }
}

let showingOverview = false;
let overviewShowStart;
let overviewFrames;
let overviewLatency;
let mallocUsedSize = 0;
let overviewShowCount = 0;
let firstOverviewUsedSize;

function script_overviewShowStart(time) {
    showingOverview = true;
    overviewShowStart = time;
    overviewFrames = 0;
}

function script_overviewShowDone(time) {
    showingOverview = false;

    overviewShowCount++;

    if (overviewShowCount == 1) {
        METRICS.overviewLatencyFirst.value = overviewLatency;
        METRICS.overviewFramesFirst.value = overviewFrames;
    } else {
        METRICS.overviewLatencySubsequent.value = overviewLatency;
        METRICS.overviewFramesSubsequent.value = overviewFrames;
    }
}

function script_afterShowHide(time) {
    if (overviewShowCount == 1) {
        METRICS.usedAfterOverview.value = mallocUsedSize;
    } else {
        METRICS.leakedAfterOverview.value = mallocUsedSize - METRICS.usedAfterOverview.value;
    }
}

function malloc_usedSize(time, bytes) {
    mallocUsedSize = bytes;
}

function clutter_stagePaintDone(time) {
    if (showingOverview) {
        if (overviewFrames == 0)
            overviewLatency = time - overviewShowStart;

        overviewFrames++;
    }
}
