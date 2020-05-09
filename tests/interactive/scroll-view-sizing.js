// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const UI = imports.testcommon.ui;

const { Clutter, GObject, Gtk, Shell, St } = imports.gi;

// This is an interactive test of the sizing behavior of StScrollView. It
// may be interesting in the future to split out the two classes at the
// top into utility classes for testing the sizing behavior of other
// containers and actors.

/****************************************************************************/

// FlowedBoxes: This is a simple actor that demonstrates an interesting
// height-for-width behavior. A set of boxes of different sizes are line-wrapped
// horizontally with the minimum horizontal size being determined by the
// largest box. It would be easy to extend this to allow doing vertical
// wrapping instead, if you wanted to see just how badly our width-for-height
// implementation is or work on fixing it.

const BOX_HEIGHT = 20;
const BOX_WIDTHS = [
    10, 40, 100, 20, 60, 30, 70, 10, 20, 200, 50, 70, 90, 20, 40,
    10, 40, 100, 20, 60, 30, 70, 10, 20, 200, 50, 70, 90, 20, 40,
    10, 40, 100, 20, 60, 30, 70, 10, 20, 200, 50, 70, 90, 20, 40,
    10, 40, 100, 20, 60, 30, 70, 10, 20, 200, 50, 70, 90, 20, 40,
];

const SPACING = 10;

var FlowedBoxes = GObject.registerClass(
class FlowedBoxes extends St.Widget {
    _init() {
        super._init();

	for (let i = 0; i < BOX_WIDTHS.length; i++) {
	    let child = new St.Bin({ width: BOX_WIDTHS[i], height: BOX_HEIGHT,
	                             style: 'border: 1px solid #444444; background: #00aa44' });
	    this.add_actor(child);
	}
    }

    vfunc_get_preferred_width(forHeight) {
        let children = this.get_children();

	let maxMinWidth = 0;
	let totalNaturalWidth = 0;

	for (let i = 0; i < children.length; i++) {
	    let child = children[i];
	    let [minWidth, naturalWidth] = child.get_preferred_width(-1);
	    maxMinWidth = Math.max(maxMinWidth, minWidth);
	    if (i != 0)
		totalNaturalWidth += SPACING;
	    totalNaturalWidth += naturalWidth;
	}

        return [maxMinWidth, totalNaturalWidth];
    }

    _layoutChildren(forWidth, callback) {
        let children = this.get_children();

	let x = 0;
	let y = 0;
	for (let i = 0; i < children.length; i++) {
	    let child = children[i];
	    let [minWidth, naturalWidth] = child.get_preferred_width(-1);
	    let [minHeight, naturalHeight] = child.get_preferred_height(naturalWidth);

	    let x1 = x;
	    if (x != 0)
		x1 += SPACING;
	    let x2 = x1 + naturalWidth;

	    if (x2 > forWidth) {
		if (x > 0) {
	            x1 = 0;
		    y += BOX_HEIGHT + SPACING;
                }

                x2 = naturalWidth;
	    }

	    callback(child, x1, y, x2, y + naturalHeight);
	    x = x2;
	}

    }

    vfunc_get_preferred_height(forWidth) {
	let height = 0;
	this._layoutChildren(forWidth,
           function(child, x1, y1, x2, y2) {
	       height = Math.max(height, y2);
	   });

        return [height, height];
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

	this._layoutChildren(box.x2 - box.x1,
           function(child, x1, y1, x2, y2) {
	       child.allocate(new Clutter.ActorBox({ x1: x1, y1: y1, x2: x2, y2: y2 }));
	   });
    }
});

/****************************************************************************/

// SizingIllustrator: this is a container that allows interactively exploring
// the sizing behavior of the child. Lines are drawn to indicate the minimum
// and natural size of the child, and a drag handle allows the user to resize
// the child interactively and see how that affects it.
//
// This is currently only written for the case where the child is height-for-width

var SizingIllustrator = GObject.registerClass(
class SizingIllustrator extends St.Widget {
    _init() {
        super._init();

	this.minWidthLine = new St.Bin({ style: 'background: red' });
        this.add_actor(this.minWidthLine);
	this.minHeightLine = new St.Bin({ style: 'background: red' });
        this.add_actor(this.minHeightLine);

	this.naturalWidthLine = new St.Bin({ style: 'background: #4444ff' });
        this.add_actor(this.naturalWidthLine);
	this.naturalHeightLine = new St.Bin({ style: 'background: #4444ff' });
        this.add_actor(this.naturalHeightLine);

	this.currentWidthLine = new St.Bin({ style: 'background: #aaaaaa' });
        this.add_actor(this.currentWidthLine);
	this.currentHeightLine = new St.Bin({ style: 'background: #aaaaaa' });
        this.add_actor(this.currentHeightLine);

	this.handle = new St.Bin({ style: 'background: yellow; border: 1px solid black;',
				   reactive: true });
	this.handle.connect('button-press-event', this._handlePressed.bind(this));
	this.handle.connect('button-release-event', this._handleReleased.bind(this));
	this.handle.connect('motion-event', this._handleMotion.bind(this));
        this.add_actor(this.handle);

	this._inDrag = false;

	this.width = 300;
	this.height = 300;
    }

    add(child) {
        this.child = child;
        this.add_child(child);
        this.set_child_below_sibling(child, null);
    }

    vfunc_get_preferred_width(forHeight) {
        let children = this.get_children();
	for (let i = 0; i < children.length; i++) {
	    let child = children[i];
	    let [minWidth, naturalWidth] = child.get_preferred_width(-1);
	    if (child == this.child) {
		this.minWidth = minWidth;
		this.naturalWidth = naturalWidth;
	    }
	}

        return [0, 400];
    }

    vfunc_get_preferred_height(forWidth) {
        let children = this.get_children();
	for (let i = 0; i < children.length; i++) {
	    let child = children[i];
	    if (child == this.child) {
		[this.minHeight, this.naturalHeight] = child.get_preferred_height(this.width);
	    } else {
		let [minWidth, naturalWidth] = child.get_preferred_height(naturalWidth);
	    }
	}

        return [0, 400];
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        box = this.get_theme_node().get_content_box(box);

	let allocWidth = box.x2 - box.x1;
	let allocHeight = box.y2 - box.y1;

	function alloc(child, x1, y1, x2, y2) {
	    child.allocate(new Clutter.ActorBox({ x1: x1, y1: y1, x2: x2, y2: y2 }));
	}

	alloc(this.child, 0, 0, this.width, this.height);
	alloc(this.minWidthLine, this.minWidth, 0, this.minWidth + 1, allocHeight);
	alloc(this.naturalWidthLine, this.naturalWidth, 0, this.naturalWidth + 1, allocHeight);
	alloc(this.currentWidthLine, this.width, 0, this.width + 1, allocHeight);
	alloc(this.minHeightLine, 0, this.minHeight, allocWidth, this.minHeight + 1);
	alloc(this.naturalHeightLine, 0, this.naturalHeight, allocWidth, this.naturalHeight + 1);
	alloc(this.currentHeightLine, 0, this.height, allocWidth, this.height + 1);
	alloc(this.handle, this.width, this.height, this.width + 10, this.height + 10);
    }

    _handlePressed(handle, event) {
	if (event.get_button() == 1) {
	    this._inDrag = true;
	    let [handleX, handleY] = handle.get_transformed_position();
	    let [x, y] = event.get_coords();
	    this._dragX = x - handleX;
	    this._dragY = y - handleY;
	}
    }

    _handleReleased(handle, event) {
	if (event.get_button() == 1) {
	    this._inDrag = false;
	}
    }

    _handleMotion(handle, event) {
	if (this._inDrag) {
	    let [x, y] = event.get_coords();
            let [actorX, actorY] = this.get_transformed_position();
	    this.width = x - this._dragX - actorX;
	    this.height = y - this._dragY - actorY;
            this.queue_relayout();
	}
    }
});

/****************************************************************************/

function test() {
    let stage = new Clutter.Stage({ width: 600, height: 600 });
    UI.init(stage);

    let mainBox = new St.BoxLayout({ width: stage.width,
				     height: stage.height,
				     vertical: true,
			             style: 'padding: 10px;'
                                     + 'spacing: 5px;'
                                     + 'font: 16px sans-serif;'
                                     + 'background: black;'
                                     + 'color: white;' });
    stage.add_actor(mainBox);

    const DOCS = 'Red lines represent minimum size, blue lines natural size. Drag yellow handle to resize ScrollView. Click on options to change.';

    let docsLabel = new St.Label({ text: DOCS });
    docsLabel.clutter_text.line_wrap = true;
    mainBox.add(docsLabel);

    let bin = new St.Bin({ x_fill: true, y_fill: true, style: 'border: 2px solid #666666;' });
    mainBox.add(bin, { x_fill: true, y_fill: true, expand: true });

    let illustrator = new SizingIllustrator();
    bin.add_actor(illustrator);

    let scrollView = new St.ScrollView();
    illustrator.add(scrollView);

    let box = new St.BoxLayout({ vertical: true });
    scrollView.add_actor(box);

    let flowedBoxes = new FlowedBoxes();
    box.add(flowedBoxes, { expand: false, x_fill: true, y_fill: true });

    let policyBox = new St.BoxLayout({ vertical: false });
    mainBox.add(policyBox);

    policyBox.add(new St.Label({ text: 'Horizontal Policy: ' }));
    let hpolicy = new St.Button({ label: 'AUTOMATIC', style: 'text-decoration: underline; color: #4444ff;' });
    policyBox.add(hpolicy);

    let spacer = new St.Bin();
    policyBox.add(spacer, { expand: true });

    policyBox.add(new St.Label({ text: 'Vertical Policy: '}));
    let vpolicy = new St.Button({ label: 'AUTOMATIC', style: 'text-decoration: underline; color: #4444ff;' });
    policyBox.add(vpolicy);

    function togglePolicy(button) {
        switch(button.label) {
        case 'AUTOMATIC':
	    button.label = 'ALWAYS';
	    break;
        case 'ALWAYS':
	    button.label = 'NEVER';
	    break;
        case 'NEVER':
	    button.label = 'EXTERNAL';
	    break;
        case 'EXTERNAL':
	    button.label = 'AUTOMATIC';
	    break;
        }
        scrollView.set_policy(Gtk.PolicyType[hpolicy.label], Gtk.PolicyType[vpolicy.label]);
    }

    hpolicy.connect('clicked', () => { togglePolicy(hpolicy); });
    vpolicy.connect('clicked', () => { togglePolicy(vpolicy); });

    let fadeBox = new St.BoxLayout({ vertical: false });
    mainBox.add(fadeBox);

    spacer = new St.Bin();
    fadeBox.add(spacer, { expand: true });

    fadeBox.add(new St.Label({ text: 'Padding: '}));
    let paddingButton = new St.Button({ label: 'No', style: 'text-decoration: underline; color: #4444ff;padding-right:3px;' });
    fadeBox.add(paddingButton);

    fadeBox.add(new St.Label({ text: 'Borders: '}));
    let borderButton = new St.Button({ label: 'No', style: 'text-decoration: underline; color: #4444ff;padding-right:3px;' });
    fadeBox.add(borderButton);

    fadeBox.add(new St.Label({ text: 'Vertical Fade: '}));
    let vfade = new St.Button({ label: 'No', style: 'text-decoration: underline; color: #4444ff;' });
    fadeBox.add(vfade);

    fadeBox.add(new St.Label({ text: 'Overlay scrollbars: '}));
    let overlay = new St.Button({ label: 'No', style: 'text-decoration: underline; color: #4444ff;' });
    fadeBox.add(overlay);

    function togglePadding(button) {
        switch(button.label) {
        case 'No':
	    button.label = 'Yes';
	    break;
        case 'Yes':
	    button.label = 'No';
	    break;
        }
        if (scrollView.style == null)
            scrollView.style = (button.label == 'Yes' ? 'padding: 10px;' : 'padding: 0;');
        else
            scrollView.style += (button.label == 'Yes' ? 'padding: 10px;' : 'padding: 0;');
    }

    paddingButton.connect('clicked', () => { togglePadding(paddingButton); });

    function toggleBorders(button) {
        switch(button.label) {
        case 'No':
	    button.label = 'Yes';
	    break;
        case 'Yes':
	    button.label = 'No';
	    break;
        }
        if (scrollView.style == null)
            scrollView.style = (button.label == 'Yes' ? 'border: 2px solid red;' : 'border: 0;');
        else
            scrollView.style += (button.label == 'Yes' ? 'border: 2px solid red;' : 'border: 0;');
    }

    borderButton.connect('clicked', () => { toggleBorders(borderButton); });

    function toggleFade(button) {
        switch(button.label) {
        case 'No':
	    button.label = 'Yes';
	    break;
        case 'Yes':
	    button.label = 'No';
	    break;
        }
        scrollView.set_style_class_name(button.label == 'Yes' ? 'vfade' : '');
    }

    vfade.connect('clicked', () => { toggleFade(vfade); });
    toggleFade(vfade);

    function toggleOverlay(button) {
        switch(button.label) {
        case 'No':
	    button.label = 'Yes';
	    break;
        case 'Yes':
	    button.label = 'No';
	    break;
        }
        scrollView.overlay_scrollbars = (button.label == 'Yes');
    }

    overlay.connect('clicked', () => { toggleOverlay(overlay); });

    UI.main(stage);
}
test();
