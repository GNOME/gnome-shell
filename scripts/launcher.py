from optparse import OptionParser
import os
import re
import subprocess
import sys

GLXINFO_RE = re.compile(r"^(\S.*):\s*\n((?:^\s+.*\n)*)", re.MULTILINE)

def _get_glx_extensions():
    """Return a tuple of server, client, and effective GLX extensions"""

    glxinfo = subprocess.Popen(["glxinfo"], stdout=subprocess.PIPE)
    glxinfo_output = glxinfo.communicate()[0]
    glxinfo.wait()

    glxinfo_map = {}
    for m in GLXINFO_RE.finditer(glxinfo_output):
        glxinfo_map[m.group(1)] = m.group(2)

    server_glx_extensions = set(re.split("\s*,\s*", glxinfo_map['server glx extensions'].strip()))
    client_glx_extensions = set(re.split("\s*,\s*", glxinfo_map['client glx extensions'].strip()))
    glx_extensions = set(re.split("\s*,\s*", glxinfo_map['GLX extensions'].strip()))

    return (server_glx_extensions, client_glx_extensions, glx_extensions)

class Launcher:
    def __init__(self):
        self.use_tfp = True
        
        # Figure out the path to the plugin when uninstalled
        scripts_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
        top_dir = os.path.dirname(scripts_dir)
        self.plugin_dir = os.path.join(top_dir, "src")
        self.js_dir = os.path.join(top_dir, "js")
        self.data_dir = os.path.join(top_dir, "data")

        parser = OptionParser()
        parser.add_option("-g", "--debug", action="store_true",
                          help="Run under a debugger")
        parser.add_option("", "--debug-command", metavar="COMMAND",
                          help="Command to use for debugging (defaults to 'gdb --args')")
        parser.add_option("-v", "--verbose", action="store_true")

        self.options, args = parser.parse_args()

        if args:
            parser.print_usage()
            sys.exit(1)

        if self.options.debug_command:
            self.options.debug = True
            self.debug_command = self.options.debug_command.split()
        else:
            self.debug_command = ["gdb", "--args"]

    def set_use_tfp(self, use_tfp):
        """Sets whether we try to use GLX_EXT_texture_for_pixmap"""
        
        self.use_tfp = use_tfp

    def start_shell(self):
        """Starts gnome-shell. Returns a subprocess.Popen object"""

        use_tfp = self.use_tfp
        force_indirect = False

        # Allow disabling usage of the EXT_texture_for_pixmap extension.
        # FIXME: Move this to ClutterGlxPixmap like
        # CLUTTER_PIXMAP_TEXTURE_RECTANGLE=disable.
        if 'GNOME_SHELL_DISABLE_TFP' in os.environ and \
           os.environ['GNOME_SHELL_DISABLE_TFP'] != '':
           use_tfp = False

        if use_tfp:
            # Decide if we need to set LIBGL_ALWAYS_INDIRECT=1 to get the texture_from_pixmap
            # extension; we take having the extension be supported on both the client and
            # server but not in the list of effective extensions as a signal of needing
            # to force indirect rendering.
            #
            # (The Xepyhr DRI support confuses this heuristic but we disable TFP in
            # start-in-Xepyhr)
            #
            (server_glx_extensions, client_glx_extensions, glx_extensions) = _get_glx_extensions()

            if ("GLX_EXT_texture_from_pixmap" in server_glx_extensions and
                "GLX_EXT_texture_from_pixmap" in client_glx_extensions and
                (not "GLX_EXT_texture_from_pixmap" in glx_extensions)):

                force_indirect = True

        # Now launch metacity-clutter with our plugin
        env=dict(os.environ)
        env.update({'GNOME_SHELL_JS'      : self.js_dir,
                    'GNOME_SHELL_DATADIR' : self.data_dir,
                    'GI_TYPELIB_PATH'     : self.plugin_dir,
                    'LD_LIBRARY_PATH'     : os.environ.get('LD_LIBRARY_PATH', '') + ':' + self.plugin_dir,
                    'GNOME_DISABLE_CRASH_DIALOG' : '1'})

        if force_indirect:
            if self.options.verbose:
                print "Forcing indirect GL"

            # This is Mesa specific; the NVIDIA proprietary drivers drivers use
            # __GL_FORCE_INDIRECT=1 instead. But we don't need to force indirect
            # rendering for NVIDIA.
            env['LIBGL_ALWAYS_INDIRECT'] = '1'

        if not self.options.verbose:
            # Unless verbose() is specified, only let gjs show errors and things that are
            # explicitly logged via log() from javascript
            env['GJS_DEBUG_TOPICS'] = 'JS ERROR;JS LOG'

        if self.options.debug:
            args = list(self.debug_command)
        else:
            args = []
                
        plugin = os.path.join(self.plugin_dir, "libgnome-shell.la")
        args.extend(['metacity', '--mutter-plugins=' + plugin, '--replace'])
        return subprocess.Popen(args, env=env)
    
    def is_verbose (self):
        """Returns whether the Launcher was started in verbose mode"""
        return self.options.verbose        
        

        
