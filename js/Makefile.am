
EXTRA_DIST = misc/config.js.in
CLEANFILES = misc/config.js

misc/config.js: misc/config.js.in Makefile
	[ -d $(@D) ] || $(mkdir_p) $(@D) ; \
	sed -e "s|[@]PACKAGE_NAME@|$(PACKAGE_NAME)|g" \
	    -e "s|[@]PACKAGE_VERSION@|$(PACKAGE_VERSION)|g" \
	    -e "s|[@]GJS_VERSION@|$(GJS_VERSION)|g" \
	    -e "s|[@]HAVE_BLUETOOTH@|$(HAVE_BLUETOOTH)|g" \
	    -e "s|[@]SHELL_SYSTEM_CA_FILE@|$(SHELL_SYSTEM_CA_FILE)|g" \
	    -e "s|[@]GETTEXT_PACKAGE@|$(GETTEXT_PACKAGE)|g" \
	    -e "s|[@]datadir@|$(datadir)|g" \
	    -e "s|[@]libexecdir@|$(libexecdir)|g" \
	    -e "s|[@]sysconfdir@|$(sysconfdir)|g" \
               $< > $@

jsdir = $(pkgdatadir)/js

nobase_dist_js_DATA = 	\
	gdm/batch.js		\
	gdm/consoleKit.js	\
	gdm/fingerprint.js	\
	gdm/loginDialog.js	\
	gdm/powerMenu.js	\
	gdm/systemd.js	 	\
	extensionPrefs/main.js	\
	misc/config.js		\
	misc/extensionUtils.js	\
	misc/fileUtils.js	\
	misc/format.js		\
	misc/gnomeSession.js	\
	misc/history.js		\
	misc/jsParse.js		\
	misc/modemManager.js	\
	misc/params.js		\
	misc/screenSaver.js     \
	misc/util.js		\
	perf/core.js		\
	ui/altTab.js		\
	ui/appDisplay.js	\
	ui/appFavorites.js	\
	ui/automountManager.js  \
	ui/autorunManager.js    \
	ui/boxpointer.js	\
	ui/calendar.js		\
	ui/checkBox.js		\
	ui/contactDisplay.js \
	ui/ctrlAltTab.js	\
	ui/dash.js		\
	ui/dateMenu.js		\
	ui/dnd.js		\
	ui/endSessionDialog.js	\
	ui/environment.js	\
	ui/extensionSystem.js	\
	ui/flashspot.js		\
	ui/iconGrid.js		\
	ui/keyboard.js		\
	ui/keyringPrompt.js	\
	ui/layout.js		\
	ui/lightbox.js		\
	ui/link.js		\
	ui/lookingGlass.js	\
	ui/magnifier.js		\
	ui/magnifierDBus.js	\
	ui/main.js		\
	ui/messageTray.js	\
	ui/modalDialog.js	\
	ui/networkAgent.js	\
	ui/shellEntry.js	\
	ui/shellMountOperation.js \
	ui/notificationDaemon.js \
	ui/overview.js		\
	ui/panel.js		\
	ui/panelMenu.js		\
	ui/placeDisplay.js	\
	ui/polkitAuthenticationAgent.js \
	ui/popupMenu.js		\
	ui/remoteSearch.js	\
	ui/runDialog.js		\
	ui/scripting.js		\
	ui/search.js		\
	ui/searchDisplay.js	\
	ui/shellDBus.js		\
	ui/statusIconDispatcher.js	\
	ui/status/accessibility.js	\
	ui/status/keyboard.js	\
	ui/status/network.js	\
	ui/status/power.js	\
	ui/status/volume.js	\
	ui/status/bluetooth.js	\
	ui/telepathyClient.js	\
	ui/tweener.js		\
	ui/userMenu.js		\
	ui/viewSelector.js	\
	ui/wanda.js		\
	ui/windowAttentionHandler.js	\
	ui/windowManager.js	\
	ui/workspace.js		\
	ui/workspaceThumbnail.js	\
	ui/workspacesView.js	\
	ui/workspaceSwitcherPopup.js    \
	ui/xdndHandler.js
