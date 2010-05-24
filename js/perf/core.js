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
    overviewFpsFirst:
    { description: "Frame rate when going to the overview, first time",
      units: "frames / s" },
    overviewLatencySubsequent:
    { description: "Time to first frame after triggering overview, second time",
      units: "us"},
    overviewFpsSubsequent:
    { description: "Frames rate when going to the overview, second time",
      units: "frames / s" },
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

    Main.overview.connect('shown', function() {
                              Scripting.scriptEvent('overviewShowDone');
                          });

    yield Scripting.sleep(1000);
    yield Scripting.waitLeisure();
    for (let i = 0; i < 2; i++) {
        Scripting.scriptEvent('overviewShowStart');
        Main.overview.show();

        yield Scripting.waitLeisure();
        Main.overview.hide();
        yield Scripting.waitLeisure();

        global.gc();
        yield Scripting.sleep(1000);
        Scripting.collectStatistics();
        Scripting.scriptEvent('afterShowHide');
    }
}

let showingOverview = false;
let finishedShowingOverview = false;
let overviewShowStart;
let overviewFrames;
let overviewLatency;
let mallocUsedSize = 0;
let overviewShowCount = 0;
let firstOverviewUsedSize;
let haveSwapComplete = false;

function script_overviewShowStart(time) {
    showingOverview = true;
    finishedShowingOverview = false;
    overviewShowStart = time;
    overviewFrames = 0;
}

function script_overviewShowDone(time) {
    // We've set up the state at the end of the zoom out, but we
    // need to wait for one more frame to paint before we count
    // ourselves as done.
    finishedShowingOverview = true;
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

function _frameDone(time) {
    if (showingOverview) {
        if (overviewFrames == 0)
            overviewLatency = time - overviewShowStart;

        overviewFrames++;
    }

    if (finishedShowingOverview) {
        showingOverview = false;
        finishedShowingOverview = false;
        overviewShowCount++;

        let dt = (time - (overviewShowStart + overviewLatency)) / 1000000;

        // If we see a start frame and an end frame, that would
        // be 1 frame for a FPS computation, hence the '- 1'
        let fps = (overviewFrames - 1) / dt;

        if (overviewShowCount == 1) {
            METRICS.overviewLatencyFirst.value = overviewLatency;
            METRICS.overviewFpsFirst.value = fps;
        } else {
            METRICS.overviewLatencySubsequent.value = overviewLatency;
            METRICS.overviewFpsSubsequent.value = fps;
        }
    }
}

function glx_swapComplete(time, swapTime) {
    haveSwapComplete = true;

    _frameDone(swapTime);
}

function clutter_stagePaintDone(time) {
    // If we aren't receiving GLXBufferSwapComplete events, then we approximate
    // the time the user sees a frame with the time we finished doing drawing
    // commands for the frame. This doesn't take into account the time for
    // the GPU to finish painting, and the time for waiting for the buffer
    // swap, but if this are uniform - every frame takes the same time to draw -
    // then it won't upset our FPS calculation, though the latency value
    // will be slightly too low.

    if (!haveSwapComplete)
        _frameDone(time);
}
