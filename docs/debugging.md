# Debugging

## Looking Glass

[Looking Glass][lg] is a built-in debugger and inspector tool that makes
it possible to run arbitrary JS code inside the shell, visually select
any object from the shell's scene, and much more.

It can be accessed by entering `lg` into the run dialog (Alt+F2).

[lg]: looking-glass.md

## Javascript stacktraces

gnome-shell is split between C and JavaScript. It is sometimes necessary
to debug the interaction between the two, for example when javascript
code triggers a warning or crash on the C side.

In gdb, the javascript stacktrace can be printed with
```
<gdb> gjs_dumpstack()
```

The `SHELL_DEBUG` environment variable can be used to print
the javascript stack automatically:

 * `backtrace-warning`: when a warning is logged
 * `backtrace-segfault`: on segfaults

To log a stacktrace when some particular javascript code is reached, you
can insert the following code:

```js
console.trace('trace from doSomething()')
```

## Debugging the session's gnome-shell proces

It is possible to attach gdb to the gnome-shell process of the existing
login session (or with the native backend on a tty), but beware: When the
debugger stops the gnome-shell process, the entire session will freeze.

It is necessary to use a second system to continue controlling the
debugger, most conveniently in combination with [screen]:

 1. create a screen session
 2. detach the session
 3. ssh from a second system
 4. resume the screen session
 5. attach gdb to the gnome-shell process

[screen]: https://www.gnu.org/software/screen/
