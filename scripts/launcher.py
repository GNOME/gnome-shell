from optparse import OptionParser
import os
import subprocess
import sys

class Launcher:
    def __init__(self):
        self.use_tfp = True
        
        # Figure out the path to the plugin when uninstalled
        scripts_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
        top_dir = os.path.dirname(scripts_dir)
        self.plugin_dir = os.path.join(top_dir, "src")
        self.js_dir = os.path.join(top_dir, "js")

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

        # Allow disabling usage of the EXT_texture_for_pixmap extension.
        # FIXME: Move this to ClutterGlxPixmap like
        # CLUTTER_PIXMAP_TEXTURE_RECTANGLE=disable.
        if 'GNOME_SHELL_DISABLE_DISABLE_TFP' in os.environ and
           os.environ['GNOME_SHELL_DISABLE_DISABLE_TFP'] != '':
           use_tfp = False

        if use_tfp:
            # Check if GLX supports GL_ARB_texture_non_power_of_two; currently clutter
            # can only use GLX_EXT_texture_for_pixmap if we have that extension.
            glxinfo = subprocess.Popen(["glxinfo"], stdout=subprocess.PIPE)
            glxinfo_output = glxinfo.communicate()[0]
            glxinfo.wait()

            use_tfp = "GL_ARB_texture_non_power_of_two" in glxinfo_output

        # Now launch metacity-clutter with our plugin
        env=dict(os.environ)
        env.update({'GNOME_SHELL_JS'  : self.js_dir,
                    'GI_TYPELIB_PATH' : self.plugin_dir,
                    'LD_LIBRARY_PATH' : os.environ.get('LD_LIBRARY_PATH', '') + ':' + self.plugin_dir,
                    'GNOME_DISABLE_CRASH_DIALOG' : '1'})

        if use_tfp:
            # If we have NPOT textures, then we want to use GLX_EXT_texture_from_pixmap; in
            # most cases, we have to force indirect rendering to do this. DRI2 will lift
            # that restriction; in which case what we should do is parse the information
            # from glxinfo more carefully... if the extension isn't listed under
            # "GLX extensions" with LIBGL_ALWAYS_INDIRECT unset, then set LIBGL_ALWAYS_INDIRECT
            # and try again.
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
        

        
