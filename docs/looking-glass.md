# Looking Glass

Looking Glass is GNOME Shell's integrated debugger and inspector tool.

You currently run it by pressing <kbd>Alt</kbd> <kbd>F2</kbd>,
and typing `lg`<kbd>Return</kbd>.

You can leave Looking Glass by pressing <kbd>Esc</kbd> in its
Evaluator pane.

Looking Glass has five major panes (Evaluator, Windows, Extensions,
Actors and Flags) and one tool (the Picker).

## Evaluator

This is an interactive JavaScript prompt. You can type arbitrary
JavaScript at the prompt, and it will be evaluated. Try, in order:

```js
1+1

global.get_window_actors()

global.get_window_actors().forEach(w => w.ease({duration: 3000, mode: Clutter.AnimationMode.EASE_OUT_QUAD, scale_x: 0.3, scale_y: 0.3}))

global.get_window_actors().forEach(w => w.set_scale(1, 1))

global.get_window_actors()[0]

it.scale_x
```

### Special evaluator features

This last bit, the `it`, deserves more explanation. One thing about the
Evaluator that's different from say Python's default interactive prompt is
that each computed value is saved by default, and can be referred back to.
Typing r(*number* ) will get you back the result with that number, and `it`
is the last result.

The evaluator also has a history feature; you can press <kbd>↑</kbd> and
<kbd>↓</kbd> to access previously used entries. The history is automatically
saved to the dconf key `/org/gnome/shell/looking-glass-history`, and loaded
when you restart the shell.

### Imports

Some common modules like Clutter, GObject or Main are pre-imported and always
available.

Additional modules can be loaded using dynamic imports:
```js
Util = await import('../misc/util.js')
{Panel} = await import('./panel.js')
{default: Pango} = await import('gi://Pango')
```

### Slowing Down Animations

You can use Looking Glass to access the global `St.Settings` object and set
its `slow_down_factor` property:

```js
St.Settings.get().slow_down_factor = 5
```

Any value greater than 1 makes all shell animations slower. This is
particularly useful when implementing a new animation behavior.

## Windows

This is a list of all open windows and related information,
like the associated app.

## Extensions

This is a list of all currently installed extensions. You can use
the View Source link to quickly access the extension folder.

## Actors

This pane gives access to a complete tree of the actor hierarchy.
It is useful when an actor cannot be selected with the picker or
accessed via code in the evaluator.

## Flags

This pane provides easy access to a list of Mutter and Clutter debug flags.

## The Picker

The picker allows you to visually select any object from the shell's scene.
When you do, it's inserted as a result into the Evaluator pane.

During picking, you can scroll to move up and down the actor hierarchy.
