// Start always in the custom homepage
user_pref("browser.startup.page", 0); // 0=Blank, 1=Home, 3=Resume
user_pref("browser.startup.homepage", "about:blank");

// Disable the initial configuration workflow and welcome screens
user_pref("browser.aboutwelcome.enabled", false);
user_pref("browser.shell.checkDefaultBrowser", false);

// Disable homepage override on updates (prevents "What's new" tabs)
user_pref("browser.startup.homepage_override.mstone", "ignore");

// Do not ask to restore the session after restarting or crashing
user_pref("browser.sessionstore.resume_from_crash", false);
user_pref("browser.sessionstore.max_resumed_crashes", 0);
user_pref("toolkit.startup.max_resumed_crashes", -1);
user_pref("browser.startup.couldRestoreSession.count", 0);

// Disable sending Firefox usage, health, and telemetry data
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("datareporting.usage.uploadEnabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("toolkit.telemetry.unified", false);
user_pref("toolkit.telemetry.archive.enabled", false);
user_pref("toolkit.telemetry.reportingpolicy.firstRun", false);

// Hide the "Firefox automatically sends some data..." info bar
user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);

// Disable "Shield" studies and background experiments
user_pref("app.shield.optoutstudies.enabled", false);
user_pref("app.normandy.enabled", false);

// Disable Crash Reporting prompts
user_pref("breakpad.reportURL", "");
user_pref("browser.tabs.crashReporting.sendReport", false);

// Do not remember or generate passwords
user_pref("signon.rememberSignons", false);
user_pref("signon.generation.enabled", false);
user_pref("signon.management.page.breach-alerts.enabled", false);

// Disable form autofill and address caching
user_pref("browser.formfill.enable", false);
user_pref("extensions.formautofill.addresses.enabled", false);
user_pref("extensions.formautofill.creditCards.enabled", false);

// Disable Pocket integration
user_pref("extensions.pocket.enabled", false);

// Disable Reader View (removes icon from URL bar)
user_pref("reader.parse-on-load.enabled", false);

// Disable Picture-in-Picture video toggles
user_pref("media.videocontrols.picture-in-picture.video-toggle.enabled", false);

// Disable HTML5 Fullscreen warning overlay
user_pref("full-screen-api.warning.timeout", 0);
user_pref("full-screen-api.transition-duration.enter", "0 0");
user_pref("full-screen-api.transition-duration.leave", "0 0");

// Disable Search Suggestions and "One Click" search
user_pref("browser.search.suggest.enabled", false);
user_pref("browser.urlbar.suggest.searches", false);

// Disable downloading the binary H.264 codec from Cisco
user_pref("media.gmp-gmpopenh264.enabled", false);

// Disable WebRTC (prevents IP leaks and unneeded p2p capabilities)
user_pref("media.peerconnection.enabled", false);

// Disable Geolocation
user_pref("geo.enabled", false);

// Disable Link Prefetching (Privacy & Bandwidth)
user_pref("network.prefetch-next", false);
user_pref("network.dns.disablePrefetch", true);

// Disable Captive Portal detection (stops pings to check for wifi login)
user_pref("network.captive-portal-service.enabled", false);

// Force new windows to open in the current tab/window (Critical for Kiosk)
// 3 = Open in new tab (which we hide via CSS), 1 = Current tab
user_pref("browser.link.open_newwindow", 3);
user_pref("browser.link.open_newwindow.restriction", 0);

// Globally disable extensions autoupdate
user_pref("extensions.update.autoUpdateDefault", false);

// Disable application auto-updates
user_pref("app.update.auto", false);
user_pref("app.update.service.enabled", false);

// Allow custom css style from chrome/userChrome.css
user_pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);
