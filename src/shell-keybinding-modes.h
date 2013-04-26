/**
 * ShellKeyBindingMode:
 * @SHELL_KEYBINDING_MODE_NONE: block keybinding
 * @SHELL_KEYBINDING_MODE_NORMAL: allow keybinding when in window mode,
 *     e.g. when the focus is in an application window
 * @SHELL_KEYBINDING_MODE_OVERVIEW: allow keybinding while the overview
 *     is active
 * @SHELL_KEYBINDING_MODE_LOCK_SCREEN: allow keybinding when the screen
 *     is locked, e.g. when the screen shield is shown
 * @SHELL_KEYBINDING_MODE_UNLOCK_SCREEN: allow keybinding in the unlock
 *     dialog
 * @SHELL_KEYBINDING_MODE_LOGIN_SCREEN: allow keybinding in the login screen
 * @SHELL_KEYBINDING_MODE_MESSAGE_TRAY: allow keybinding while the message
 *     tray is popped up
 * @SHELL_KEYBINDING_MODE_SYSTEM_MODAL: allow keybinding when a system modal
 *     dialog (e.g. authentification or session dialogs) is open
 * @SHELL_KEYBINDING_MODE_LOOKING_GLASS: allow keybinding in looking glass
 * @SHELL_KEYBINDING_MODE_TOPBAR_POPUP: allow keybinding while a top bar menu
 *     is open
 * @SHELL_KEYBINDING_MODE_ALL: always allow keybinding
 *
 * Controls in which GNOME Shell states a keybinding should be handled.
*/
typedef enum {
  SHELL_KEYBINDING_MODE_NONE          = 0,
  SHELL_KEYBINDING_MODE_NORMAL        = 1 << 0,
  SHELL_KEYBINDING_MODE_OVERVIEW      = 1 << 1,
  SHELL_KEYBINDING_MODE_LOCK_SCREEN   = 1 << 2,
  SHELL_KEYBINDING_MODE_UNLOCK_SCREEN = 1 << 3,
  SHELL_KEYBINDING_MODE_LOGIN_SCREEN  = 1 << 4,
  SHELL_KEYBINDING_MODE_MESSAGE_TRAY  = 1 << 5,
  SHELL_KEYBINDING_MODE_SYSTEM_MODAL  = 1 << 6,
  SHELL_KEYBINDING_MODE_LOOKING_GLASS = 1 << 7,
  SHELL_KEYBINDING_MODE_TOPBAR_POPUP  = 1 << 8,

  SHELL_KEYBINDING_MODE_ALL = ~0,
} ShellKeyBindingMode;

