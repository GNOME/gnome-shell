# Coding guide

Our goal is to have all JavaScript code in GNOME follow a consistent style. In
a dynamic language like JavaScript, it is essential to be rigorous about style
(and unit tests), or you rapidly end up with a spaghetti-code mess.

## A quick note

Life isn't fun if you can't break the rules. If a rule seems unnecessarily
restrictive while you're coding, ignore it, and let the patch reviewer decide
what to do.

## Indentation, braces and whitespace

* Use four-space indents.
* Braces are on the same line as their associated statements.
* You should only omit braces if *both* sides of the statement are on one line.
* One space after the `function` keyword.
* No space between the function name in a declaration or a call.
* One space before the parens in the `if` statements, or `while`, or `for` loops.

```javascript
    function foo(a, b) {
        let bar;

        if (a > b)
            bar = do_thing(a);
        else
            bar = do_thing(b);

        if (var == 5) {
            for (let i = 0; i < 10; i++)
                print(i);
        } else {
            print(20);
        }
    }
```

## Semicolons

JavaScript allows omitting semicolons at the end of lines, but don't. Always
end statements with a semicolon.

## js2-mode

If using Emacs, do not use js2-mode. It is outdated and hasn't worked for a
while. emacs now has a built-in JavaScript mode, js-mode, based on
espresso-mode. It is the de facto emacs mode for JavaScript.

## File naming and creation

For JavaScript files, use lowerCamelCase-style names, with a `.js` extension.

We only use C where gjs/gobject-introspection is not available for the task, or
where C would be cleaner. To work around limitations in
gjs/gobject-introspection itself, add a new method in `shell-util.[ch]`.

Like many other GNOME projects, we prefix our C source filenames with the
library name followed by a dash, e.g. `shell-app-system.c`. Create a
`-private.h` header when you want to share code internally in the
library. These headers are not installed, distributed or introspected.

## Imports

Use UpperCamelCase when importing modules to distinguish them from ordinary
variables, e.g.
```javascript
    const GLib = imports.gi.GLib;
```
Imports should be categorized into one of two places. The top-most import block
should contain only "environment imports". These are either modules from
gobject-introspection or modules added by gjs itself.

The second block of imports should contain only "application imports". These
are the JS code that is in the gnome-shell codebase,
e.g. `imports.ui.popupMenu`.

Each import block should be sorted alphabetically. Don't import modules you
don't use.
```javascript
    const { GLib, Gio, St } = imports.gi;

    const Main = imports.ui.main;
    const Params = imports.misc.params;
    const Util = imports.misc.util;
```
The alphabetical ordering should be done independently of the location of the
location. Never reference `imports` in actual code.

## Constants

We use CONSTANTS_CASE to define constants. All constants should be directly
under the imports:
```javascript
    const MY_DBUS_INTERFACE = 'org.my.Interface';
```

## Variable declaration

Always use either `const` or `let` when defining a variable.
```javascript
    // Iterating over an array
    for (let i = 0; i < arr.length; ++i)
        let item = arr[i];

    // Iterating over an object's properties
    for (let prop in someobj) {
        ...
    }
```

If you use "var" then the variable is added to function scope, not block scope.
See [What's new in JavaScript 1.7](https://developer.mozilla.org/en/JavaScript/New_in_JavaScript/1.7#Block_scope_with_let_%28Merge_into_let_Statement%29)

## Classes

There are many approaches to classes in JavaScript. We use standard ES6 classes
whenever possible, that is when not inheriting from GObjects.
```javascript
    var IconLabelMenuItem = class extends PopupMenu.PopupMenuBaseItem {
        constructor(icon, label) {
            super({ reactive: false });
            this.actor.add_child(icon);
            this.actor.add_child(label);
        }

        open() {
            log("menu opened!");
        }
    };
```

For GObject inheritance, we use the GObject.registerClass() function provided
by gjs.
```javascript
    var MyActor = GObject.registerClass(
    class MyActor extends Clutter.Actor {
        _init(params) {
            super._init(params);

            this.name = 'MyCustomActor';
        }
    });
```

## GObject Introspection

GObject Introspection is a powerful feature that allows us to have native
bindings for almost any library built around GObject. If a library requires
you to inherit from a type to use it, you can do so:
```javascript
    var MyClutterActor = GObject.registerClass(
    class MyClutterActor extends Clutter.Actor {

        vfunc_get_preferred_width(forHeight) {
             return [100, 100];
        }

        vfunc_get_preferred_height(forWidth) {
             return [100, 100];
        }

        vfunc_paint(paintContext) {
             let framebuffer = paintContext.get_framebuffer();
             let coglContext = framebuffer.get_context();
             let alloc = this.get_allocation_box();

             let pipeline = new Cogl.Pipeline(coglContext);
             pipeline.set_color4ub(255, 0, 0, 255);

             framebuffer.draw_rectangle(pipeline,
		 alloc.x1, alloc.y1,
		 alloc.x2, alloc.y2);
        }
    });
```

## Translatable strings, `environment.js`

We use gettext to translate the GNOME Shell into all the languages that GNOME
supports. The `gettext` function is aliased globally as `_`, you do not need to
explicitly import it. This is done through some magic in the
[environment.js](http://git.gnome.org/browse/gnome-shell/tree/js/ui/environment.js)
file. If you can't find a method that's used, it's probably either in gjs itself
or installed on the global object from the Environment.

Use 'single quotes' for programming strings that should not be translated
and "double quotes" for strings that the user may see. This allows us to
quickly find untranslated or mistranslated strings by grepping through the
sources for double quotes without a gettext call around them.

## `actor` (deprecated) and `_delegate`

gjs allows us to set so-called "expando properties" on introspected objects,
allowing us to treat them like any other. Because the Shell was built before
you could inherit from GTypes natively in JS, in some cases we have a wrapper
class that has a property called `actor` (now deprecated). We call this
wrapper class the "delegate".

We sometimes use expando properties to set a property called `_delegate` on
the actor itself:
```javascript
    var MyActor = GObject.registerClass(
    class MyActor extends Clutter.Actor {
        _init(params) {
            super._init(params);
            this._delegate = this;
        }
    });
```

Or using the deprecated `actor`:
```javascript
    var MyClass = class {
        constructor() {
            this.actor = new St.Button({ text: "This is a button" });
            this.actor._delegate = this;

            this.actor.connect('clicked', this._onClicked.bind(this));
        }

        _onClicked(actor) {
            actor.set_label("You clicked the button!");
        }
    };
```

The 'delegate' property is important for anything which trying to get the
delegate object from an associated actor. For instance, the drag and drop
system calls the `handleDragOver` function on the delegate of a "drop target"
when the user drags an item over it. If you do not set the `_delegate`
property, your actor will not be able to be dropped onto.
In case the class is an actor itself, the `_delegate` can be just set to `this`.

## Functional style

JavaScript Array objects offer a lot of common functional programming
capabilities such as forEach, map, filter and so on. You can use these when
they make sense, but please don't have a spaghetti mess of function programming
messed in a procedural style. Use your best judgment.

## Closures

`this` will not be captured in a closure, it is relative to how the closure is
invoked, not to the value of this where the closure is created, because "this"
is a keyword with a value passed in at function invocation time, it is not a
variable that can be captured in closures.

All closures should be wrapped with Function.prototype.bind or use arrow
notation.
```javascript
    let closure1 = () => this._fnorbate();
    let closure2 = this._fnorbate.bind(this);
```

A more realistic example would be connecting to a signal on a method of a
prototype:
```javascript
    const FnorbLib = imports.fborbLib;

    var MyClass = class {
        _init() {
            let fnorb = new FnorbLib.Fnorb();
            fnorb.connect('frobate', this._onFnorbFrobate.bind(this));
        }

        _onFnorbFrobate(fnorb) {
            this._updateFnorb();
        }
    };
```

## Object literal syntax

In JavaScript, these are equivalent:
```javascript
    foo = { 'bar': 42 };
    foo = { bar: 42 };
```

and so are these:
```javascript
    var b = foo['bar'];
    var b = foo.bar;
```

If your usage of an object is like an object, then you're defining "member
variables." For member variables, use the no-quotes no-brackets syntax: `{ bar:
42 }` `foo.bar`.

If your usage of an object is like a hash table (and thus conceptually the keys
can have special chars in them), don't use quotes, but use brackets: `{ bar: 42
}`, `foo['bar']`.

## Animations

Most objects that are animated are actors, and most properties used in animations
are animatable, which means they can use implicit animations:

```javascript
    moveActor(actor, x, y) {
        actor.ease({
            x,
            y,
            duration: 500, // ms
            mode: Clutter.AnimationMode.EASE_OUT_QUAD
        });
    }
```

The above is a convenience wrapper around the actual Clutter API, and should generally
be preferred over the more verbose:

```javascript
    moveActor(actor, x, y) {
        actor.save_easing_state();

        actor.set_easing_duration(500);
        actor.set_easing_mode(Clutter.AnimationMode.EASE_OUT_QUAD);
        actor.set({
            x,
            y
        });

        actor.restore_easing_state();
    }
```

There is a similar convenience API around Clutter.PropertyTransition to animate
actor (or actor meta) properties that cannot use implicit animations:

```javascript
    desaturateActor(actor, desaturate) {
        let factor = desaturate ? 1.0 : 0.0;
        actor.ease_property('@effects.desaturate.factor', factor, {
            duration: 500, // ms
            mode: Clutter.AnimationMode.EASE_OUT_QUAD
        });
    }
```
